/*
 * namespace.c - Linux namespace setup / Linux 命名空间设置
 *
 * Configures mount, PID, network, UTS, and cgroup namespaces
 * for the Android container.
 * 为 Android 容器配置挂载、PID、网络、UTS 和 cgroup 命名空间。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include "container.h"

/* === Create namespaces via unshare / 通过 unshare 创建命名空间 === */

int ns_create(uint32_t flags)
{
    int clone_flags = 0;
    if (flags & NS_MOUNT)  clone_flags |= CLONE_NEWNS;
    if (flags & NS_PID)    clone_flags |= CLONE_NEWPID;
    if (flags & NS_NET)    clone_flags |= CLONE_NEWNET;
    if (flags & NS_IPC)    clone_flags |= CLONE_NEWIPC;
    if (flags & NS_UTS)    clone_flags |= CLONE_NEWUTS;
    if (flags & NS_CGROUP) clone_flags |= CLONE_NEWCGROUP;

    if (unshare(clone_flags) < 0) {
        fprintf(stderr, "ERROR: unshare(0x%x) failed: %s\n",
                clone_flags, strerror(errno));
        return -1;
    }
    return 0;
}

/* === Setup mount namespace / 设置挂载命名空间 === */

int ns_setup_mount(void)
{
    /* Make all mounts private (don't propagate to host) */
    /* 使所有挂载私有（不传播到宿主） */
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
        fprintf(stderr, "ERROR: mount MS_PRIVATE failed: %s\n", strerror(errno));
        return -1;
    }

    /* Mount proc for new PID namespace / 为新 PID 命名空间挂载 proc */
    mkdir("/proc", 0755);
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) < 0) {
        if (errno != EBUSY) {
            fprintf(stderr, "WARNING: mount /proc failed: %s\n", strerror(errno));
        }
    }

    /* Mount tmpfs for /dev / 为 /dev 挂载 tmpfs */
    if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755,size=64k") < 0) {
        if (errno != EBUSY) {
            fprintf(stderr, "WARNING: mount /dev failed: %s\n", strerror(errno));
        }
    }

    return 0;
}

/* === Setup PID namespace / 设置 PID 命名空间 === */

int ns_setup_pid(void)
{
    /* PID namespace is already created by clone(), just mount /proc */
    /* PID 命名空间已由 clone() 创建，只需挂载 /proc */
    return 0;
}

/* === Setup network namespace / 设置网络命名空间 === */

int ns_setup_network(bool share_host)
{
    if (share_host) {
        /* Shared network: nothing to do / 共享网络：无需操作 */
        return 0;
    }

    /*
     * Create veth pair for bridged networking.
     * 创建 veth 对用于桥接网络。
     *
     * TODO: use netlink to create veth0/veth1 pair
     * TODO: 使用 netlink 创建 veth0/veth1 对
     *
     * For now, configure loopback only.
     * 目前只配置回环。
     */

    /* Bring up loopback / 启动回环 */
    int ret = system("ip link set lo up 2>/dev/null");
    (void)ret;

    return 0;
}

/* === Setup UTS namespace (hostname) / 设置 UTS 命名空间（主机名） === */

int ns_setup_uts(const char *hostname)
{
    if (sethostname(hostname, strlen(hostname)) < 0) {
        fprintf(stderr, "WARNING: sethostname failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/* === Setup cgroup for container / 为容器设置 cgroup === */

int ns_setup_cgroup(const char *name, int64_t mem_limit, int32_t cpu_shares)
{
    return ct_cgroup_set_limits(name, mem_limit, cpu_shares);
}
