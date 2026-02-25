/*
 * mount.c - Filesystem mounting / 文件系统挂载
 *
 * Mounts essential virtual filesystems during early boot,
 * and parses fstab for additional mounts.
 * 在早期启动时挂载必要的虚拟文件系统，并解析 fstab 进行额外挂载。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "lumid.h"

/*
 * Mount table for early boot virtual filesystems.
 * 早期启动虚拟文件系统的挂载表。
 */
struct mount_entry {
    const char *source;
    const char *target;
    const char *fstype;
    unsigned long flags;
    const char *options;
};

static const struct mount_entry early_mounts[] = {
    /* procfs - process info / 进程信息 */
    { "proc",     "/proc",          "proc",     MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL },
    /* sysfs - kernel objects / 内核对象 */
    { "sysfs",    "/sys",           "sysfs",    MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL },
    /* devtmpfs - device nodes / 设备节点 */
    { "devtmpfs", "/dev",           "devtmpfs", MS_NOSUID,                         "mode=0755" },
    /* devpts - pseudo-terminals / 伪终端 */
    { "devpts",   "/dev/pts",       "devpts",   MS_NOSUID | MS_NOEXEC,             "mode=0620,ptmxmode=0666" },
    /* tmpfs for /dev/shm - shared memory / 共享内存 */
    { "tmpfs",    "/dev/shm",       "tmpfs",    MS_NOSUID | MS_NODEV,              "mode=1777" },
    /* tmpfs for /run - runtime data / 运行时数据 */
    { "tmpfs",    "/run",           "tmpfs",    MS_NOSUID | MS_NODEV,              "mode=0755" },
    /* tmpfs for /tmp - temporary files / 临时文件 */
    { "tmpfs",    "/tmp",           "tmpfs",    MS_NOSUID | MS_NODEV,              "mode=1777" },
    /* cgroup2 - unified cgroup hierarchy / 统一 cgroup 层次结构 */
    { "cgroup2",  "/sys/fs/cgroup", "cgroup2",  MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL },
    /* Sentinel / 哨兵 */
    { NULL, NULL, NULL, 0, NULL }
};

/* === Mount initial filesystems / 挂载初始文件系统 === */

int mount_initial_filesystems(void)
{
    int failed = 0;

    for (const struct mount_entry *m = early_mounts; m->source; m++) {
        /* Create mount point if it doesn't exist / 如果挂载点不存在则创建 */
        struct stat st;
        if (stat(m->target, &st) < 0) {
            if (util_mkdir_p(m->target, 0755) < 0) {
                LOG_E("failed to create mount point %s: %s",
                      m->target, strerror(errno));
                failed++;
                continue;
            }
        }

        /* Skip if already mounted / 如果已挂载则跳过 */
        if (mount(m->source, m->target, m->fstype, m->flags, m->options) < 0) {
            if (errno == EBUSY) {
                LOG_D("%s already mounted, skipping", m->target);
                continue;
            }
            LOG_E("failed to mount %s on %s (%s): %s",
                  m->source, m->target, m->fstype, strerror(errno));
            failed++;
            continue;
        }

        LOG_D("mounted %s on %s (type %s)", m->source, m->target, m->fstype);
    }

    /* Create essential device nodes if devtmpfs didn't / 如果 devtmpfs 未创建则手动创建设备节点 */
    struct stat st;
    if (stat("/dev/null", &st) < 0) {
        mknod("/dev/null",    S_IFCHR | 0666, makedev(1, 3));
        mknod("/dev/zero",    S_IFCHR | 0666, makedev(1, 5));
        mknod("/dev/random",  S_IFCHR | 0666, makedev(1, 8));
        mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9));
        mknod("/dev/console", S_IFCHR | 0600, makedev(5, 1));
        mknod("/dev/tty",     S_IFCHR | 0666, makedev(5, 0));
        LOG_D("created essential device nodes");
    }

    /* Create symlinks / 创建符号链接 */
    symlink("/proc/self/fd",   "/dev/fd");
    symlink("/proc/self/fd/0", "/dev/stdin");
    symlink("/proc/self/fd/1", "/dev/stdout");
    symlink("/proc/self/fd/2", "/dev/stderr");

    if (failed > 0) {
        LOG_W("%d early mounts failed", failed);
        return -1;
    }

    LOG_I("early filesystems mounted successfully");
    return 0;
}

/* === Parse and mount fstab entries / 解析并挂载 fstab 条目 === */

int mount_fstab(const char *fstab_path)
{
    FILE *fp = fopen(fstab_path, "r");
    if (!fp) {
        LOG_W("fstab not found: %s", fstab_path);
        return -1;
    }

    char line[LUMID_MAX_LINE_LEN];
    int mounted = 0;

    while (fgets(line, sizeof(line), fp)) {
        /* Skip comments and empty lines / 跳过注释和空行 */
        char *l = line;
        while (*l == ' ' || *l == '\t')
            l++;
        if (*l == '#' || *l == '\n' || *l == '\0')
            continue;

        /*
         * Parse fstab fields: source target fstype options dump pass
         * 解析 fstab 字段: 源 目标 文件系统类型 选项 dump pass
         */
        char source[256], target[256], fstype[64], options[256];
        int dump, pass;

        if (sscanf(l, "%255s %255s %63s %255s %d %d",
                   source, target, fstype, options, &dump, &pass) < 4) {
            continue;
        }

        /* Skip already-mounted virtual filesystems / 跳过已挂载的虚拟文件系统 */
        if (strcmp(fstype, "proc") == 0 || strcmp(fstype, "sysfs") == 0 ||
            strcmp(fstype, "devtmpfs") == 0 || strcmp(fstype, "tmpfs") == 0 ||
            strcmp(fstype, "cgroup2") == 0) {
            continue;
        }

        /* Create mount point / 创建挂载点 */
        util_mkdir_p(target, 0755);

        /* Parse mount flags from options string / 从选项字符串解析挂载标志 */
        unsigned long flags = 0;
        const char *data = NULL;

        if (strstr(options, "ro"))
            flags |= MS_RDONLY;
        if (strstr(options, "noexec"))
            flags |= MS_NOEXEC;
        if (strstr(options, "nosuid"))
            flags |= MS_NOSUID;
        if (strstr(options, "nodev"))
            flags |= MS_NODEV;
        if (strcmp(options, "defaults") != 0)
            data = options;

        if (mount(source, target, fstype, flags, data) < 0) {
            LOG_E("fstab: failed to mount %s on %s: %s",
                  source, target, strerror(errno));
        } else {
            LOG_I("fstab: mounted %s on %s (type %s)", source, target, fstype);
            mounted++;
        }
    }

    fclose(fp);
    LOG_I("fstab: mounted %d additional filesystems", mounted);
    return 0;
}
