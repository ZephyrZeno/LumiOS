/*
 * util.c - Utility functions / 工具函数
 *
 * Common helper functions used throughout lumid.
 * lumid 中通用的辅助函数。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "lumid.h"

/* === Get monotonic time in nanoseconds / 获取单调时间（纳秒） === */

uint64_t util_monotonic_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* === Get human-readable timestamp / 获取可读时间戳 === */

const char *util_timestamp(void)
{
    static char buf[32];
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

/* === Recursive mkdir -p / 递归创建目录 === */

int util_mkdir_p(const char *path, mode_t mode)
{
    char tmp[LUMID_MAX_PATH_LEN];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    /* Remove trailing slash / 移除尾部斜杠 */
    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) < 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }

    if (mkdir(tmp, mode) < 0 && errno != EEXIST)
        return -1;

    return 0;
}

/* === Write string to file / 将字符串写入文件 === */

int util_write_file(const char *path, const char *content)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return -1;

    size_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    close(fd);

    return (written == (ssize_t)len) ? 0 : -1;
}

/* === Read entire file into malloc'd buffer / 读取整个文件到 malloc 分配的缓冲区 === */

char *util_read_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;

    /* Get file size / 获取文件大小 */
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0 || size > 1024 * 1024) { /* 1MB max / 最大 1MB */
        close(fd);
        return NULL;
    }
    lseek(fd, 0, SEEK_SET);

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        close(fd);
        return NULL;
    }

    ssize_t n = read(fd, buf, (size_t)size);
    close(fd);

    if (n < 0) {
        free(buf);
        return NULL;
    }

    buf[n] = '\0';
    return buf;
}

/*
 * Parse human-readable size string (e.g., "512M", "2G", "1024K").
 * 解析人类可读的大小字符串 (如 "512M", "2G", "1024K")。
 */
int util_parse_size(const char *str, int64_t *out)
{
    if (!str || !out)
        return -1;

    char *end;
    int64_t val = strtoll(str, &end, 10);

    if (end == str)
        return -1;

    /* Parse suffix / 解析后缀 */
    switch (toupper((unsigned char)*end)) {
    case 'K':
        val *= 1024LL;
        break;
    case 'M':
        val *= 1024LL * 1024LL;
        break;
    case 'G':
        val *= 1024LL * 1024LL * 1024LL;
        break;
    case 'T':
        val *= 1024LL * 1024LL * 1024LL * 1024LL;
        break;
    case '\0':
    case '\n':
    case '\r':
        break; /* No suffix, treat as bytes / 无后缀，视为字节 */
    default:
        return -1;
    }

    *out = val;
    return 0;
}

/* === Daemonize process / 守护进程化 === */

int util_daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid > 0)
        _exit(0); /* Parent exits / 父进程退出 */

    /* Create new session / 创建新会话 */
    if (setsid() < 0)
        return -1;

    /* Second fork to prevent acquiring controlling terminal */
    /* 第二次 fork 防止获取控制终端 */
    pid = fork();
    if (pid < 0)
        return -1;
    if (pid > 0)
        _exit(0);

    /* Redirect stdio to /dev/null / 重定向标准 IO 到 /dev/null */
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO)
            close(devnull);
    }

    return 0;
}

/*
 * Drop root privileges to specified user/group.
 * 降权到指定的用户/组。
 */
int util_drop_privileges(const char *user, const char *group)
{
    if (!user || user[0] == '\0')
        return 0;

    /* Lookup user / 查找用户 */
    struct passwd *pw = getpwnam(user);
    if (!pw) {
        LOG_E("user '%s' not found", user);
        return -1;
    }

    /* Set group / 设置组 */
    gid_t gid = pw->pw_gid;
    if (group && group[0] != '\0') {
        struct group *gr = getgrnam(group);
        if (gr) {
            gid = gr->gr_gid;
        } else {
            LOG_W("group '%s' not found, using user's primary group", group);
        }
    }

    /* Set supplementary groups / 设置附加组 */
    if (initgroups(user, gid) < 0) {
        LOG_W("failed to set supplementary groups for '%s': %s",
              user, strerror(errno));
    }

    /* Set GID then UID (order matters!) / 先设 GID 再设 UID（顺序重要！） */
    if (setgid(gid) < 0) {
        LOG_E("failed to set gid %d: %s", gid, strerror(errno));
        return -1;
    }

    if (setuid(pw->pw_uid) < 0) {
        LOG_E("failed to set uid %d: %s", pw->pw_uid, strerror(errno));
        return -1;
    }

    LOG_D("dropped privileges to %s:%s (uid=%d gid=%d)",
          user, group ? group : "(default)",
          pw->pw_uid, gid);

    return 0;
}
