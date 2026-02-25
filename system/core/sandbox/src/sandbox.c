/*
 * sandbox.c - Core sandbox framework / 核心沙箱框架
 *
 * Applies three-layer security: Landlock + seccomp + AppArmor.
 * 应用三层安全: Landlock + seccomp + AppArmor。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "sandbox.h"

int sbx_init(void)
{
    fprintf(stderr, "[sandbox] initializing security framework v%s\n", SANDBOX_VERSION);
    fprintf(stderr, "[sandbox] landlock: %s\n", landlock_supported() ? "supported" : "not supported");
    fprintf(stderr, "[sandbox] apparmor: %s\n", apparmor_supported() ? "supported" : "not supported");
    return 0;
}

void sbx_policy_default(sbx_policy_t *policy, sbx_level_t level)
{
    memset(policy, 0, sizeof(*policy));
    policy->level = level;

    switch (level) {
    case SBX_LEVEL_NONE:
        policy->net_flags = SBX_NET_FULL;
        policy->ipc_flags = SBX_IPC_PIPE | SBX_IPC_SOCKET | SBX_IPC_SHM | SBX_IPC_DBUS;
        policy->hw_flags  = 0xFF;
        break;

    case SBX_LEVEL_TRUSTED:
        /* Filesystem limits only / 仅文件系统限制 */
        landlock_add_path(policy, "/usr",  SBX_FS_READ | SBX_FS_EXECUTE);
        landlock_add_path(policy, "/etc",  SBX_FS_READ);
        landlock_add_path(policy, "/tmp",  SBX_FS_READ | SBX_FS_WRITE | SBX_FS_CREATE);
        policy->net_flags = SBX_NET_FULL;
        policy->ipc_flags = SBX_IPC_PIPE | SBX_IPC_SOCKET | SBX_IPC_DBUS;
        policy->hw_flags  = 0xFF;
        break;

    case SBX_LEVEL_NORMAL:
        /* Standard app sandbox / 标准应用沙箱 */
        landlock_add_path(policy, "/usr",    SBX_FS_READ | SBX_FS_EXECUTE);
        landlock_add_path(policy, "/etc",    SBX_FS_READ);
        landlock_add_path(policy, "/tmp",    SBX_FS_READ | SBX_FS_WRITE | SBX_FS_CREATE);
        landlock_add_path(policy, "/dev/null", SBX_FS_READ | SBX_FS_WRITE);
        landlock_add_path(policy, "/dev/urandom", SBX_FS_READ);
        seccomp_preset_normal(policy);
        policy->net_flags = SBX_NET_TCP_CONNECT | SBX_NET_DNS;
        policy->ipc_flags = SBX_IPC_PIPE | SBX_IPC_DBUS;
        policy->hw_flags  = SBX_HW_NONE;
        policy->max_fds   = 256;
        policy->max_pids  = 16;
        break;

    case SBX_LEVEL_STRICT:
        /* Strict: no network / 严格: 无网络 */
        landlock_add_path(policy, "/usr",    SBX_FS_READ | SBX_FS_EXECUTE);
        landlock_add_path(policy, "/etc",    SBX_FS_READ);
        landlock_add_path(policy, "/dev/null", SBX_FS_READ | SBX_FS_WRITE);
        landlock_add_path(policy, "/dev/urandom", SBX_FS_READ);
        seccomp_preset_strict(policy);
        policy->net_flags = SBX_NET_NONE;
        policy->ipc_flags = SBX_IPC_PIPE;
        policy->hw_flags  = SBX_HW_NONE;
        policy->max_fds   = 64;
        policy->max_pids  = 4;
        break;

    case SBX_LEVEL_UNTRUSTED:
        /* Maximum isolation / 最大隔离 */
        landlock_add_path(policy, "/usr/lib", SBX_FS_READ);
        landlock_add_path(policy, "/dev/null", SBX_FS_READ | SBX_FS_WRITE);
        seccomp_preset_strict(policy);
        policy->net_flags   = SBX_NET_NONE;
        policy->ipc_flags   = SBX_IPC_NONE;
        policy->hw_flags    = SBX_HW_NONE;
        policy->max_memory  = 256 * 1024 * 1024; /* 256 MB */
        policy->max_fds     = 32;
        policy->max_pids    = 2;
        policy->max_cpu_pct = 50;
        break;
    }
}

int sbx_apply(const sbx_policy_t *policy)
{
    int ret = 0;

    fprintf(stderr, "[sandbox] applying policy '%s' level=%d\n",
            policy->name, policy->level);

    if (policy->level == SBX_LEVEL_NONE) {
        fprintf(stderr, "[sandbox] no sandbox (system service)\n");
        return 0;
    }

    /* Layer 1: Landlock filesystem isolation / 第一层: 文件系统隔离 */
    if (landlock_supported() && policy->num_path_rules > 0) {
        ret = landlock_apply(policy);
        if (ret < 0)
            fprintf(stderr, "[sandbox] WARNING: landlock failed: %s\n", strerror(errno));
        else
            fprintf(stderr, "[sandbox] landlock: %d path rules applied\n", policy->num_path_rules);
    }

    /* Layer 2: seccomp syscall filter / 第二层: 系统调用过滤 */
    if (policy->num_syscall_rules > 0) {
        ret = seccomp_apply(policy);
        if (ret < 0)
            fprintf(stderr, "[sandbox] WARNING: seccomp failed: %s\n", strerror(errno));
        else
            fprintf(stderr, "[sandbox] seccomp: %d rules applied\n", policy->num_syscall_rules);
    }

    /* Layer 3: AppArmor MAC / 第三层: 强制访问控制 */
    if (policy->apparmor_profile[0] && apparmor_supported()) {
        ret = apparmor_apply(policy->apparmor_profile);
        if (ret < 0)
            fprintf(stderr, "[sandbox] WARNING: apparmor failed: %s\n", strerror(errno));
        else
            fprintf(stderr, "[sandbox] apparmor: profile '%s' applied\n", policy->apparmor_profile);
    }

    fprintf(stderr, "[sandbox] policy applied (net=0x%x ipc=0x%x hw=0x%x)\n",
            policy->net_flags, policy->ipc_flags, policy->hw_flags);
    return 0;
}

int sbx_policy_load(sbx_policy_t *policy, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    memset(policy, 0, sizeof(*policy));
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;

        char key[64], val[256];
        if (sscanf(line, "%63[^=]=%255s", key, val) != 2) continue;

        if (strcmp(key, "name") == 0)
            strncpy(policy->name, val, sizeof(policy->name) - 1);
        else if (strcmp(key, "level") == 0)
            policy->level = atoi(val);
        else if (strcmp(key, "net") == 0)
            policy->net_flags = (uint32_t)strtoul(val, NULL, 0);
        else if (strcmp(key, "ipc") == 0)
            policy->ipc_flags = (uint32_t)strtoul(val, NULL, 0);
        else if (strcmp(key, "hw") == 0)
            policy->hw_flags = (uint32_t)strtoul(val, NULL, 0);
        else if (strcmp(key, "apparmor") == 0)
            strncpy(policy->apparmor_profile, val, sizeof(policy->apparmor_profile) - 1);
        else if (strcmp(key, "max_memory") == 0)
            policy->max_memory = strtoull(val, NULL, 0);
        else if (strcmp(key, "max_pids") == 0)
            policy->max_pids = (uint32_t)atoi(val);
        else if (strcmp(key, "max_fds") == 0)
            policy->max_fds = (uint32_t)atoi(val);
        else if (strncmp(key, "path", 4) == 0) {
            /* path=/usr:0x03 → path + flags */
            char *colon = strchr(val, ':');
            if (colon && policy->num_path_rules < SBX_MAX_PATH_RULES) {
                *colon = '\0';
                sbx_path_rule_t *r = &policy->path_rules[policy->num_path_rules++];
                strncpy(r->path, val, sizeof(r->path) - 1);
                r->flags = (uint32_t)strtoul(colon + 1, NULL, 0);
            }
        }
    }

    fclose(f);
    return 0;
}

int sbx_policy_save(const sbx_policy_t *policy, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "# LumiOS sandbox policy\n");
    fprintf(f, "name=%s\n", policy->name);
    fprintf(f, "level=%d\n", policy->level);
    fprintf(f, "net=0x%x\n", policy->net_flags);
    fprintf(f, "ipc=0x%x\n", policy->ipc_flags);
    fprintf(f, "hw=0x%x\n", policy->hw_flags);

    if (policy->apparmor_profile[0])
        fprintf(f, "apparmor=%s\n", policy->apparmor_profile);
    if (policy->max_memory)
        fprintf(f, "max_memory=%lu\n", (unsigned long)policy->max_memory);
    if (policy->max_pids)
        fprintf(f, "max_pids=%u\n", policy->max_pids);
    if (policy->max_fds)
        fprintf(f, "max_fds=%u\n", policy->max_fds);

    for (int i = 0; i < policy->num_path_rules; i++) {
        fprintf(f, "path=%s:0x%x\n",
                policy->path_rules[i].path, policy->path_rules[i].flags);
    }

    fclose(f);
    return 0;
}
