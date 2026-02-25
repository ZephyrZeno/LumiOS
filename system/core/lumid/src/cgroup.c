/*
 * cgroup.c - Cgroup v2 management / cgroup v2 管理
 *
 * Creates and manages cgroups for service resource isolation.
 * 为服务资源隔离创建和管理 cgroups。
 *
 * Uses unified cgroup v2 hierarchy mounted at /sys/fs/cgroup.
 * 使用挂载在 /sys/fs/cgroup 的统一 cgroup v2 层次结构。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "lumid.h"

/* === Initialize cgroup subsystem / 初始化 cgroup 子系统 === */

int cgroup_init(void)
{
    struct stat st;

    /* Check if cgroup v2 is mounted / 检查 cgroup v2 是否已挂载 */
    if (stat("/sys/fs/cgroup/cgroup.controllers", &st) < 0) {
        LOG_W("cgroup v2 not available at /sys/fs/cgroup");
        return -1;
    }

    /* Create lumid cgroup root / 创建 lumid cgroup 根目录 */
    if (util_mkdir_p(LUMID_CGROUP_ROOT, 0755) < 0) {
        LOG_E("failed to create cgroup root: %s: %s",
              LUMID_CGROUP_ROOT, strerror(errno));
        return -1;
    }

    /*
     * Enable controllers in subtree.
     * 在子树中启用控制器。
     * Write "+cpu +memory +io +pids" to cgroup.subtree_control
     */
    char subtree_path[LUMID_MAX_PATH_LEN];
    snprintf(subtree_path, sizeof(subtree_path),
             "%s/cgroup.subtree_control", LUMID_CGROUP_ROOT);

    if (util_write_file(subtree_path, "+cpu +memory +io +pids\n") < 0) {
        LOG_W("failed to enable cgroup controllers, some limits may not work");
    }

    LOG_I("cgroup v2 initialized at %s", LUMID_CGROUP_ROOT);
    return 0;
}

/* === Create cgroup for a service / 为服务创建 cgroup === */

int cgroup_create(const char *name)
{
    char path[LUMID_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", LUMID_CGROUP_ROOT, name);

    if (util_mkdir_p(path, 0755) < 0) {
        LOG_E("failed to create cgroup for '%s': %s", name, strerror(errno));
        return -1;
    }

    LOG_D("created cgroup: %s", path);
    return 0;
}

/* === Apply resource limits / 应用资源限制 === */

int cgroup_apply_limits(const char *name, const cgroup_limits_t *limits)
{
    char path[LUMID_MAX_PATH_LEN];
    char value[64];

    /* Memory max / 最大内存 */
    if (limits->memory_max > 0) {
        snprintf(path, sizeof(path), "%s/%s/memory.max",
                 LUMID_CGROUP_ROOT, name);
        snprintf(value, sizeof(value), "%ld", (long)limits->memory_max);
        if (util_write_file(path, value) < 0)
            LOG_W("failed to set memory.max for '%s'", name);
    }

    /* Memory high watermark / 内存高水位 */
    if (limits->memory_high > 0) {
        snprintf(path, sizeof(path), "%s/%s/memory.high",
                 LUMID_CGROUP_ROOT, name);
        snprintf(value, sizeof(value), "%ld", (long)limits->memory_high);
        if (util_write_file(path, value) < 0)
            LOG_W("failed to set memory.high for '%s'", name);
    }

    /* CPU weight / CPU 权重 */
    if (limits->cpu_weight > 0) {
        snprintf(path, sizeof(path), "%s/%s/cpu.weight",
                 LUMID_CGROUP_ROOT, name);
        snprintf(value, sizeof(value), "%d", limits->cpu_weight);
        if (util_write_file(path, value) < 0)
            LOG_W("failed to set cpu.weight for '%s'", name);
    }

    /* CPU max (bandwidth limiting) / CPU 上限 (带宽限制) */
    if (limits->cpu_max > 0 && limits->cpu_max <= 100) {
        snprintf(path, sizeof(path), "%s/%s/cpu.max",
                 LUMID_CGROUP_ROOT, name);
        /* Format: quota_us period_us (e.g., "50000 100000" for 50%) */
        snprintf(value, sizeof(value), "%d 100000",
                 limits->cpu_max * 1000);
        if (util_write_file(path, value) < 0)
            LOG_W("failed to set cpu.max for '%s'", name);
    }

    /* IO weight / IO 权重 */
    if (limits->io_weight > 0) {
        snprintf(path, sizeof(path), "%s/%s/io.weight",
                 LUMID_CGROUP_ROOT, name);
        snprintf(value, sizeof(value), "default %d", limits->io_weight);
        if (util_write_file(path, value) < 0)
            LOG_W("failed to set io.weight for '%s'", name);
    }

    /* PIDs max / 最大进程数 */
    if (limits->pids_max > 0) {
        snprintf(path, sizeof(path), "%s/%s/pids.max",
                 LUMID_CGROUP_ROOT, name);
        snprintf(value, sizeof(value), "%d", limits->pids_max);
        if (util_write_file(path, value) < 0)
            LOG_W("failed to set pids.max for '%s'", name);
    }

    LOG_D("applied cgroup limits for '%s'", name);
    return 0;
}

/* === Add PID to cgroup / 将 PID 添加到 cgroup === */

int cgroup_add_pid(const char *name, pid_t pid)
{
    char path[LUMID_MAX_PATH_LEN];
    char value[32];

    snprintf(path, sizeof(path), "%s/%s/cgroup.procs",
             LUMID_CGROUP_ROOT, name);
    snprintf(value, sizeof(value), "%d", pid);

    if (util_write_file(path, value) < 0) {
        LOG_E("failed to add pid %d to cgroup '%s': %s",
              pid, name, strerror(errno));
        return -1;
    }

    LOG_D("added pid %d to cgroup '%s'", pid, name);
    return 0;
}

/* === Remove cgroup / 移除 cgroup === */

int cgroup_remove(const char *name)
{
    char path[LUMID_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", LUMID_CGROUP_ROOT, name);

    /* rmdir will only succeed if cgroup is empty / rmdir 只在 cgroup 为空时成功 */
    if (rmdir(path) < 0) {
        if (errno != ENOENT) {
            LOG_D("could not remove cgroup '%s': %s (may still have processes)",
                  name, strerror(errno));
        }
        return -1;
    }

    LOG_D("removed cgroup '%s'", name);
    return 0;
}
