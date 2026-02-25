/*
 * seccomp.c - System call filtering / 系统调用过滤
 *
 * Uses seccomp-bpf to restrict which syscalls an application can invoke.
 * 使用 seccomp-bpf 限制应用可调用的系统调用。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <linux/audit.h>
#include <stddef.h>
#include "sandbox.h"

/* Architecture audit constant */
#if defined(__x86_64__)
#define AUDIT_ARCH_CURRENT AUDIT_ARCH_X86_64
#elif defined(__aarch64__)
#define AUDIT_ARCH_CURRENT AUDIT_ARCH_AARCH64
#elif defined(__i386__)
#define AUDIT_ARCH_CURRENT AUDIT_ARCH_I386
#else
#define AUDIT_ARCH_CURRENT 0
#endif

/* Dangerous syscalls to block in NORMAL mode / NORMAL模式阻止的危险调用 */
static const int blocked_normal[] = {
    SYS_mount, SYS_umount2, SYS_pivot_root,
    SYS_reboot, SYS_kexec_load,
    SYS_init_module, SYS_finit_module, SYS_delete_module,
    SYS_swapon, SYS_swapoff,
    SYS_sethostname, SYS_setdomainname,
    SYS_acct,
#ifdef SYS_lookup_dcookie
    SYS_lookup_dcookie,
#endif
#ifdef SYS_perf_event_open
    SYS_perf_event_open,
#endif
#ifdef SYS_bpf
    SYS_bpf,
#endif
    -1
};

/* Extra syscalls blocked in STRICT mode / STRICT模式额外阻止的调用 */
static const int blocked_strict[] = {
    SYS_socket, SYS_connect, SYS_accept, SYS_bind, SYS_listen,
    SYS_sendto, SYS_recvfrom, SYS_sendmsg, SYS_recvmsg,
    SYS_setsockopt, SYS_getsockopt,
    SYS_ptrace,
    SYS_process_vm_readv, SYS_process_vm_writev,
#ifdef SYS_userfaultfd
    SYS_userfaultfd,
#endif
#ifdef SYS_io_uring_setup
    SYS_io_uring_setup, SYS_io_uring_enter, SYS_io_uring_register,
#endif
    SYS_keyctl, SYS_request_key, SYS_add_key,
    -1
};

int seccomp_add_rule(sbx_policy_t *policy, int syscall_nr, sbx_syscall_action_t action)
{
    if (policy->num_syscall_rules >= SBX_MAX_SYSCALL_RULES) return -1;
    sbx_syscall_rule_t *r = &policy->syscall_rules[policy->num_syscall_rules++];
    r->nr = syscall_nr;
    r->action = action;
    return 0;
}

void seccomp_preset_normal(sbx_policy_t *policy)
{
    policy->syscall_whitelist = false;
    for (int i = 0; blocked_normal[i] >= 0; i++) {
        seccomp_add_rule(policy, blocked_normal[i], SBX_SYSCALL_DENY);
    }
}

void seccomp_preset_strict(sbx_policy_t *policy)
{
    seccomp_preset_normal(policy);
    for (int i = 0; blocked_strict[i] >= 0; i++) {
        seccomp_add_rule(policy, blocked_strict[i], SBX_SYSCALL_DENY);
    }
}

int seccomp_apply(const sbx_policy_t *policy)
{
    if (policy->num_syscall_rules <= 0) return 0;

    /*
     * Build BPF program:
     *   1. Verify architecture
     *   2. Load syscall number
     *   3. For each blocked syscall: JEQ → KILL_PROCESS
     *   4. Default: ALLOW
     */
    int num_rules = policy->num_syscall_rules;
    int prog_len = 3 + num_rules + 1; /* arch check + load nr + rules + default */

    struct sock_filter *filter = calloc(prog_len, sizeof(struct sock_filter));
    if (!filter) return -ENOMEM;

    int idx = 0;

    /* Load architecture / 加载架构 */
    filter[idx++] = (struct sock_filter)
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch));

    /* Verify architecture / 验证架构 */
    filter[idx++] = (struct sock_filter)
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_CURRENT, 0, prog_len - idx - 1);

    /* Load syscall number / 加载系统调用号 */
    filter[idx++] = (struct sock_filter)
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr));

    /* Add deny rules / 添加拒绝规则 */
    for (int i = 0; i < num_rules; i++) {
        const sbx_syscall_rule_t *r = &policy->syscall_rules[i];
        if (r->action == SBX_SYSCALL_DENY) {
            int remaining = num_rules - i - 1;
            filter[idx++] = (struct sock_filter)
                BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, r->nr, remaining + 1, 0);
        } else if (r->action == SBX_SYSCALL_LOG) {
            filter[idx++] = (struct sock_filter)
                BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, r->nr, 0, 0); /* pass through */
        } else {
            filter[idx++] = (struct sock_filter)
                BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, r->nr, 0, 0);
        }
    }

    /* Default: allow / 默认: 允许 */
    filter[idx++] = (struct sock_filter)
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);

    /* Kill on denied syscall / 拒绝时终止 */
    filter[idx++] = (struct sock_filter)
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS);

    struct sock_fprog prog = {
        .len = (unsigned short)idx,
        .filter = filter,
    };

    /* Require NO_NEW_PRIVS */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        fprintf(stderr, "[seccomp] prctl(NO_NEW_PRIVS): %s\n", strerror(errno));
        free(filter);
        return -1;
    }

    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) < 0) {
        fprintf(stderr, "[seccomp] set filter: %s\n", strerror(errno));
        free(filter);
        return -1;
    }

    free(filter);
    fprintf(stderr, "[seccomp] filter applied (%d rules)\n", num_rules);
    return 0;
}
