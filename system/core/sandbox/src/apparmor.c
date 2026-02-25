/*
 * apparmor.c - AppArmor Mandatory Access Control / 强制访问控制
 *
 * Applies AppArmor profiles to restrict application capabilities.
 * 应用 AppArmor 配置文件限制应用能力。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "sandbox.h"

#define APPARMOR_PROC "/proc/self/attr/current"
#define APPARMOR_PROFILES "/etc/apparmor.d"

bool apparmor_supported(void)
{
    struct stat st;
    return stat("/sys/kernel/security/apparmor", &st) == 0;
}

int apparmor_apply(const char *profile)
{
    if (!profile || !profile[0]) return -1;

    int fd = open(APPARMOR_PROC, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[apparmor] open %s: %s\n", APPARMOR_PROC, strerror(errno));
        return -1;
    }

    /* Write "changeprofile <profile>" to switch / 写入切换配置 */
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "changeprofile %s", profile);

    if (write(fd, buf, len) < 0) {
        fprintf(stderr, "[apparmor] changeprofile '%s': %s\n", profile, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    fprintf(stderr, "[apparmor] switched to profile '%s'\n", profile);
    return 0;
}

int apparmor_generate_profile(const sbx_policy_t *policy, char *buf, size_t len)
{
    if (!policy || !buf || len < 256) return -1;

    int off = 0;
    off += snprintf(buf + off, len - off,
        "#include <tunables/global>\n\n"
        "profile %s flags=(attach_disconnected) {\n"
        "  #include <abstractions/base>\n\n",
        policy->name[0] ? policy->name : "lumi-app");

    /* Filesystem rules */
    for (int i = 0; i < policy->num_path_rules; i++) {
        const sbx_path_rule_t *r = &policy->path_rules[i];
        char perms[16] = "";
        int p = 0;
        if (r->flags & SBX_FS_READ)    perms[p++] = 'r';
        if (r->flags & SBX_FS_WRITE)   perms[p++] = 'w';
        if (r->flags & SBX_FS_EXECUTE) perms[p++] = 'x';
        if (r->flags & SBX_FS_CREATE)  perms[p++] = 'c';
        if (r->flags & SBX_FS_DELETE)  perms[p++] = 'd';
        perms[p] = '\0';

        off += snprintf(buf + off, len - off,
            "  %s/** %s,\n", r->path, perms);
    }

    /* Network rules */
    if (policy->net_flags == SBX_NET_NONE) {
        off += snprintf(buf + off, len - off, "\n  deny network,\n");
    } else {
        if (policy->net_flags & SBX_NET_TCP_CONNECT)
            off += snprintf(buf + off, len - off, "  network tcp,\n");
        if (policy->net_flags & SBX_NET_UDP)
            off += snprintf(buf + off, len - off, "  network udp,\n");
    }

    /* Hardware deny rules */
    if (!(policy->hw_flags & SBX_HW_CAMERA))
        off += snprintf(buf + off, len - off, "  deny /dev/video* rw,\n");
    if (!(policy->hw_flags & SBX_HW_MIC))
        off += snprintf(buf + off, len - off, "  deny /dev/snd/* rw,\n");
    if (!(policy->hw_flags & SBX_HW_GPS))
        off += snprintf(buf + off, len - off, "  deny /dev/ttyUSB* rw,\n");
    if (!(policy->hw_flags & SBX_HW_BLUETOOTH))
        off += snprintf(buf + off, len - off, "  deny /dev/hci* rw,\n");

    off += snprintf(buf + off, len - off, "}\n");
    return off;
}
