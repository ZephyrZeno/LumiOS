/*
 * sandbox.h - LumiOS Security Sandbox / 安全沙箱框架
 *
 * Three-layer defense: Landlock (filesystem) + seccomp (syscalls) + AppArmor (MAC)
 * 三层防御: Landlock (文件系统) + seccomp (系统调用) + AppArmor (强制访问控制)
 */

#ifndef SANDBOX_H
#define SANDBOX_H

#include <stdbool.h>
#include <stdint.h>

#define SANDBOX_VERSION "0.1.0"

/* === Sandbox policy / 沙箱策略 === */

/* Filesystem access flags (Landlock) / 文件系统访问标志 */
#define SBX_FS_READ       (1 << 0)
#define SBX_FS_WRITE      (1 << 1)
#define SBX_FS_EXECUTE    (1 << 2)
#define SBX_FS_CREATE     (1 << 3)
#define SBX_FS_DELETE     (1 << 4)
#define SBX_FS_TRUNCATE   (1 << 5)

/* Network access flags / 网络访问标志 */
#define SBX_NET_NONE      0
#define SBX_NET_TCP_CONNECT (1 << 0)
#define SBX_NET_TCP_BIND    (1 << 1)
#define SBX_NET_UDP         (1 << 2)
#define SBX_NET_DNS         (1 << 3)
#define SBX_NET_FULL        0xFF

/* IPC flags / 进程间通信标志 */
#define SBX_IPC_NONE      0
#define SBX_IPC_PIPE      (1 << 0)
#define SBX_IPC_SOCKET    (1 << 1)
#define SBX_IPC_SHM       (1 << 2)
#define SBX_IPC_DBUS      (1 << 3)

/* Hardware access flags / 硬件访问标志 */
#define SBX_HW_NONE       0
#define SBX_HW_CAMERA     (1 << 0)
#define SBX_HW_MIC        (1 << 1)
#define SBX_HW_GPS        (1 << 2)
#define SBX_HW_BLUETOOTH  (1 << 3)
#define SBX_HW_SENSORS    (1 << 4)
#define SBX_HW_USB        (1 << 5)

/* Preset security levels / 预设安全等级 */
typedef enum {
    SBX_LEVEL_NONE = 0,       /* No sandbox (system services) */
    SBX_LEVEL_TRUSTED,        /* Minimal: filesystem limits only */
    SBX_LEVEL_NORMAL,         /* Standard: fs + syscall filter */
    SBX_LEVEL_STRICT,         /* Strict: fs + syscall + no network */
    SBX_LEVEL_UNTRUSTED,      /* Maximum: all restrictions */
} sbx_level_t;

/* Filesystem path rule / 文件系统路径规则 */
#define SBX_MAX_PATH_RULES 64

typedef struct {
    char     path[256];
    uint32_t flags;           /* SBX_FS_* combination */
} sbx_path_rule_t;

/* Syscall filter rule / 系统调用过滤规则 */
#define SBX_MAX_SYSCALL_RULES 128

typedef enum {
    SBX_SYSCALL_ALLOW = 0,
    SBX_SYSCALL_DENY,
    SBX_SYSCALL_LOG,          /* Allow but log / 允许但记录 */
} sbx_syscall_action_t;

typedef struct {
    int                  nr;         /* Syscall number / 系统调用号 */
    sbx_syscall_action_t action;
} sbx_syscall_rule_t;

/* Complete sandbox policy / 完整沙箱策略 */
typedef struct {
    char              name[64];       /* App/service name / 应用名 */
    sbx_level_t       level;

    /* Landlock: filesystem rules / 文件系统规则 */
    sbx_path_rule_t   path_rules[SBX_MAX_PATH_RULES];
    int               num_path_rules;

    /* seccomp: syscall filter / 系统调用过滤 */
    sbx_syscall_rule_t syscall_rules[SBX_MAX_SYSCALL_RULES];
    int               num_syscall_rules;
    bool              syscall_whitelist;  /* true=whitelist, false=blacklist */

    /* Network permissions / 网络权限 */
    uint32_t          net_flags;

    /* IPC permissions / IPC 权限 */
    uint32_t          ipc_flags;

    /* Hardware permissions / 硬件权限 */
    uint32_t          hw_flags;

    /* AppArmor profile name / AppArmor 配置名 */
    char              apparmor_profile[128];

    /* Resource limits / 资源限制 */
    uint64_t          max_memory;     /* bytes, 0=unlimited */
    uint32_t          max_pids;       /* 0=unlimited */
    uint32_t          max_fds;        /* max open file descriptors */
    uint32_t          max_cpu_pct;    /* 0-100 */
} sbx_policy_t;

/* === Function declarations / 函数声明 === */

/* sandbox.c - Core framework / 核心框架 */
int  sbx_init(void);
int  sbx_apply(const sbx_policy_t *policy);
void sbx_policy_default(sbx_policy_t *policy, sbx_level_t level);
int  sbx_policy_load(sbx_policy_t *policy, const char *path);
int  sbx_policy_save(const sbx_policy_t *policy, const char *path);

/* landlock.c - Filesystem sandboxing / 文件系统沙箱 */
int  landlock_apply(const sbx_policy_t *policy);
int  landlock_add_path(sbx_policy_t *policy, const char *path, uint32_t flags);
bool landlock_supported(void);

/* seccomp.c - System call filtering / 系统调用过滤 */
int  seccomp_apply(const sbx_policy_t *policy);
int  seccomp_add_rule(sbx_policy_t *policy, int syscall_nr, sbx_syscall_action_t action);
void seccomp_preset_normal(sbx_policy_t *policy);
void seccomp_preset_strict(sbx_policy_t *policy);

/* apparmor.c - Mandatory Access Control / 强制访问控制 */
int  apparmor_apply(const char *profile);
int  apparmor_generate_profile(const sbx_policy_t *policy, char *buf, size_t len);
bool apparmor_supported(void);

#endif /* SANDBOX_H */
