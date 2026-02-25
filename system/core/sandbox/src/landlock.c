/*
 * landlock.c - Landlock filesystem sandboxing / 文件系统沙箱
 *
 * Uses Linux Landlock LSM to restrict filesystem access per-application.
 * 使用 Linux Landlock LSM 限制每个应用的文件系统访问。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include "sandbox.h"

/* Landlock ABI constants (kernel 5.13+) */
#ifndef LANDLOCK_CREATE_RULESET
#define LANDLOCK_CREATE_RULESET      444
#define LANDLOCK_ADD_RULE            445
#define LANDLOCK_RESTRICT_SELF       446
#endif

#define LANDLOCK_ACCESS_FS_EXECUTE    (1ULL << 0)
#define LANDLOCK_ACCESS_FS_WRITE_FILE (1ULL << 1)
#define LANDLOCK_ACCESS_FS_READ_FILE  (1ULL << 2)
#define LANDLOCK_ACCESS_FS_READ_DIR   (1ULL << 3)
#define LANDLOCK_ACCESS_FS_REMOVE_DIR (1ULL << 4)
#define LANDLOCK_ACCESS_FS_REMOVE_FILE (1ULL << 5)
#define LANDLOCK_ACCESS_FS_MAKE_CHAR  (1ULL << 6)
#define LANDLOCK_ACCESS_FS_MAKE_DIR   (1ULL << 7)
#define LANDLOCK_ACCESS_FS_MAKE_REG   (1ULL << 8)
#define LANDLOCK_ACCESS_FS_MAKE_SOCK  (1ULL << 9)
#define LANDLOCK_ACCESS_FS_MAKE_FIFO  (1ULL << 10)
#define LANDLOCK_ACCESS_FS_MAKE_BLOCK (1ULL << 11)
#define LANDLOCK_ACCESS_FS_MAKE_SYM   (1ULL << 12)
#define LANDLOCK_ACCESS_FS_TRUNCATE   (1ULL << 13)

#define LANDLOCK_RULE_PATH_BENEATH  1

struct landlock_ruleset_attr {
    __u64 handled_access_fs;
};

struct landlock_path_beneath_attr {
    __u64 allowed_access;
    __s32 parent_fd;
} __attribute__((packed));

static inline int ll_create_ruleset(struct landlock_ruleset_attr *attr, size_t size, __u32 flags)
{
    return (int)syscall(LANDLOCK_CREATE_RULESET, attr, size, flags);
}

static inline int ll_add_rule(int fd, int type, void *attr, __u32 flags)
{
    return (int)syscall(LANDLOCK_ADD_RULE, fd, type, attr, flags);
}

static inline int ll_restrict_self(int fd, __u32 flags)
{
    return (int)syscall(LANDLOCK_RESTRICT_SELF, fd, flags);
}

/* Convert SBX_FS_* flags to Landlock access flags */
static __u64 sbx_to_landlock(__u32 sbx_flags)
{
    __u64 ll = 0;
    if (sbx_flags & SBX_FS_READ)
        ll |= LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR;
    if (sbx_flags & SBX_FS_WRITE)
        ll |= LANDLOCK_ACCESS_FS_WRITE_FILE;
    if (sbx_flags & SBX_FS_EXECUTE)
        ll |= LANDLOCK_ACCESS_FS_EXECUTE;
    if (sbx_flags & SBX_FS_CREATE)
        ll |= LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_DIR;
    if (sbx_flags & SBX_FS_DELETE)
        ll |= LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR;
    if (sbx_flags & SBX_FS_TRUNCATE)
        ll |= LANDLOCK_ACCESS_FS_TRUNCATE;
    return ll;
}

bool landlock_supported(void)
{
    struct landlock_ruleset_attr attr = { .handled_access_fs = 0 };
    int fd = ll_create_ruleset(&attr, sizeof(attr), 0);
    if (fd >= 0) {
        close(fd);
        return true;
    }
    return errno != ENOSYS && errno != EOPNOTSUPP;
}

int landlock_add_path(sbx_policy_t *policy, const char *path, uint32_t flags)
{
    if (policy->num_path_rules >= SBX_MAX_PATH_RULES) return -1;
    sbx_path_rule_t *r = &policy->path_rules[policy->num_path_rules++];
    strncpy(r->path, path, sizeof(r->path) - 1);
    r->flags = flags;
    return 0;
}

int landlock_apply(const sbx_policy_t *policy)
{
    if (!landlock_supported()) {
        fprintf(stderr, "[landlock] not supported on this kernel\n");
        return -1;
    }

    /* Compute full access mask from all rules */
    __u64 handled = LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_WRITE_FILE |
                    LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR |
                    LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE |
                    LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_DIR |
                    LANDLOCK_ACCESS_FS_TRUNCATE;

    struct landlock_ruleset_attr attr = { .handled_access_fs = handled };
    int ruleset_fd = ll_create_ruleset(&attr, sizeof(attr), 0);
    if (ruleset_fd < 0) {
        fprintf(stderr, "[landlock] create_ruleset failed: %s\n", strerror(errno));
        return -1;
    }

    /* Add path rules */
    for (int i = 0; i < policy->num_path_rules; i++) {
        const sbx_path_rule_t *r = &policy->path_rules[i];
        int path_fd = open(r->path, O_PATH | O_CLOEXEC);
        if (path_fd < 0) {
            fprintf(stderr, "[landlock] open '%s': %s (skipping)\n", r->path, strerror(errno));
            continue;
        }

        struct landlock_path_beneath_attr pb = {
            .allowed_access = sbx_to_landlock(r->flags),
            .parent_fd = path_fd,
        };

        if (ll_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &pb, 0) < 0) {
            fprintf(stderr, "[landlock] add_rule '%s': %s\n", r->path, strerror(errno));
        }
        close(path_fd);
    }

    /* No new privileges needed for Landlock */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        fprintf(stderr, "[landlock] prctl(NO_NEW_PRIVS): %s\n", strerror(errno));
        close(ruleset_fd);
        return -1;
    }

    /* Enforce ruleset */
    if (ll_restrict_self(ruleset_fd, 0) < 0) {
        fprintf(stderr, "[landlock] restrict_self: %s\n", strerror(errno));
        close(ruleset_fd);
        return -1;
    }

    close(ruleset_fd);
    fprintf(stderr, "[landlock] applied %d rules\n", policy->num_path_rules);
    return 0;
}
