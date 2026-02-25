/*
 * log.c - Logging subsystem / 日志子系统
 *
 * Provides structured logging to stdout/stderr and log files.
 * 提供结构化日志输出到标准输出/错误和日志文件。
 *
 * Format: [TIMESTAMP] [LEVEL] message
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "lumid.h"

/* Log state / 日志状态 */
static log_level_t g_log_level = LOG_INFO;
static int g_log_fd = -1;
static char g_log_dir[LUMID_MAX_PATH_LEN] = "";

/* Level strings / 级别字符串 */
static const char *level_str[] = {
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR",
    "FATAL",
};

/* ANSI colors for terminal output / 终端输出的 ANSI 颜色 */
static const char *level_color[] = {
    "\033[36m",  /* DEBUG - cyan / 青色 */
    "\033[32m",  /* INFO  - green / 绿色 */
    "\033[33m",  /* WARN  - yellow / 黄色 */
    "\033[31m",  /* ERROR - red / 红色 */
    "\033[35m",  /* FATAL - magenta / 品红 */
};
static const char *color_reset = "\033[0m";

/* === Initialize logging / 初始化日志 === */

void log_init(const char *log_dir)
{
    if (log_dir) {
        strncpy(g_log_dir, log_dir, sizeof(g_log_dir) - 1);
        util_mkdir_p(log_dir, 0755);

        /* Open main system log file / 打开主系统日志文件 */
        char path[LUMID_MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/lumid.log", log_dir);
        g_log_fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
        if (g_log_fd < 0) {
            fprintf(stderr, "WARNING: failed to open log file %s: %s\n",
                    path, strerror(errno));
        }
    }
}

/* === Set minimum log level / 设置最低日志级别 === */

void log_set_level(log_level_t level)
{
    g_log_level = level;
}

/* === Write a log entry / 写入日志条目 === */

void log_write(log_level_t level, const char *fmt, ...)
{
    if (level < g_log_level)
        return;

    /* Get timestamp / 获取时间戳 */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);

    char timebuf[32];
    snprintf(timebuf, sizeof(timebuf), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             ts.tv_nsec / 1000000);

    /* Format message / 格式化消息 */
    char msgbuf[LUMID_MAX_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
    va_end(ap);

    /* Write to stderr with color / 带颜色写入标准错误 */
    int is_tty = isatty(STDERR_FILENO);
    if (is_tty) {
        fprintf(stderr, "%s[%s]%s [%s] %s\n",
                level_color[level], timebuf, color_reset,
                level_str[level], msgbuf);
    } else {
        fprintf(stderr, "[%s] [%s] %s\n", timebuf, level_str[level], msgbuf);
    }

    /* Write to log file (no color) / 写入日志文件 (无颜色) */
    if (g_log_fd >= 0) {
        char linebuf[LUMID_MAX_LINE_LEN + 64];
        int len = snprintf(linebuf, sizeof(linebuf),
                           "[%s] [%s] %s\n", timebuf, level_str[level], msgbuf);
        if (len > 0) {
            write(g_log_fd, linebuf, (size_t)len);
        }
    }

    /* Fatal logs trigger abort in debug builds / Fatal 日志在调试构建中触发中止 */
#ifdef LUMID_DEBUG
    if (level == LOG_FATAL) {
        abort();
    }
#endif
}

/* === Close logging / 关闭日志 === */

void log_close(void)
{
    if (g_log_fd >= 0) {
        close(g_log_fd);
        g_log_fd = -1;
    }
}
