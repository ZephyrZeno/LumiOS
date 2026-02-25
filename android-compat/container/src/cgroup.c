/*
 * cgroup.c - Container cgroup management / 容器 cgroup 管理
 *
 * Creates and manages cgroup v2 resource limits for the Android container.
 * 为 Android 容器创建和管理 cgroup v2 资源限制。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "container.h"

#define CGROUP_ROOT "/sys/fs/cgroup/android"

/* Helper: write string to file / 辅助: 写字符串到文件 */
static int write_file(const char *path, const char *content)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    size_t len = strlen(content);
    ssize_t w = write(fd, content, len);
    close(fd);
    return (w == (ssize_t)len) ? 0 : -1;
}

/* Helper: recursive mkdir / 辅助: 递归创建目录 */
static int mkdirp(const char *path, mode_t mode)
{
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, mode);
            *p = '/';
        }
    }
    return mkdir(tmp, mode) < 0 && errno != EEXIST ? -1 : 0;
}

/* === Create cgroup for container / 为容器创建 cgroup === */

int ct_cgroup_create(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", CGROUP_ROOT, name);

    if (mkdirp(path, 0755) < 0) {
        fprintf(stderr, "WARNING: failed to create cgroup %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    /* Enable controllers / 启用控制器 */
    char ctrl_path[512];
    snprintf(ctrl_path, sizeof(ctrl_path), "%s/cgroup.subtree_control", CGROUP_ROOT);
    write_file(ctrl_path, "+cpu +memory +io +pids\n");

    fprintf(stderr, "[android-container] cgroup created: %s\n", path);
    return 0;
}

/* === Set resource limits / 设置资源限制 === */

int ct_cgroup_set_limits(const char *name, int64_t mem, int32_t cpu)
{
    char path[512];
    char value[64];

    /* Memory limit / 内存限制 */
    if (mem > 0) {
        snprintf(path, sizeof(path), "%s/%s/memory.max", CGROUP_ROOT, name);
        snprintf(value, sizeof(value), "%ld", (long)mem);
        if (write_file(path, value) < 0)
            fprintf(stderr, "WARNING: failed to set memory.max for '%s'\n", name);
    }

    /* CPU shares / CPU 份额 */
    if (cpu > 0) {
        snprintf(path, sizeof(path), "%s/%s/cpu.weight", CGROUP_ROOT, name);
        snprintf(value, sizeof(value), "%d", cpu);
        if (write_file(path, value) < 0)
            fprintf(stderr, "WARNING: failed to set cpu.weight for '%s'\n", name);
    }

    /* Default PID limit for container / 容器默认 PID 限制 */
    snprintf(path, sizeof(path), "%s/%s/pids.max", CGROUP_ROOT, name);
    write_file(path, "4096");

    return 0;
}

/* === Add PID to cgroup / 将 PID 添加到 cgroup === */

int ct_cgroup_add_pid(const char *name, pid_t pid)
{
    char path[512];
    char value[32];

    snprintf(path, sizeof(path), "%s/%s/cgroup.procs", CGROUP_ROOT, name);
    snprintf(value, sizeof(value), "%d", pid);

    if (write_file(path, value) < 0) {
        fprintf(stderr, "WARNING: failed to add pid %d to cgroup '%s'\n", pid, name);
        return -1;
    }
    return 0;
}

/* === Destroy cgroup / 销毁 cgroup === */

int ct_cgroup_destroy(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", CGROUP_ROOT, name);

    if (rmdir(path) < 0 && errno != ENOENT) {
        fprintf(stderr, "WARNING: failed to remove cgroup '%s': %s\n",
                name, strerror(errno));
        return -1;
    }
    return 0;
}
