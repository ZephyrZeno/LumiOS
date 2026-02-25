/*
 * supervisor.c - Process supervisor / 进程监控器
 *
 * Monitors running services, reaps zombies, handles restart policies.
 * 监控运行中的服务，回收僵尸进程，处理重启策略。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#include "lumid.h"

/* Supervisor state / 监控器状态 */
static uint64_t g_last_check_time = 0;
#define CHECK_INTERVAL_NS (5ULL * 1000000000ULL) /* 5 seconds / 5 秒 */

int supervisor_init(void)
{
    g_last_check_time = util_monotonic_ns();
    LOG_I("process supervisor initialized");
    return 0;
}

/*
 * Handle SIGCHLD - reap all zombie children.
 * 处理 SIGCHLD - 回收所有僵尸子进程。
 */
void supervisor_handle_sigchld(void)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /* Find which service this PID belongs to / 查找该 PID 属于哪个服务 */
        service_t *svc = service_get_list();
        service_t *found = NULL;

        while (svc) {
            if (svc->pid == pid) {
                found = svc;
                break;
            }
            svc = svc->next;
        }

        if (!found) {
            /* Orphan process reaped / 回收了孤儿进程 */
            LOG_D("reaped orphan process (pid %d)", pid);
            continue;
        }

        /* Record exit info / 记录退出信息 */
        if (WIFEXITED(status)) {
            found->exit_code = WEXITSTATUS(status);
            LOG_I("service '%s' exited (pid %d, code %d)",
                  found->name, pid, found->exit_code);
        } else if (WIFSIGNALED(status)) {
            found->exit_code = -WTERMSIG(status);
            LOG_W("service '%s' killed by signal %d (pid %d)",
                  found->name, WTERMSIG(status), pid);
        }

        found->pid = -1;
        found->stop_time = util_monotonic_ns();

        /* Determine if we should restart / 判断是否需要重启 */
        bool should_restart = false;

        switch (found->restart_policy) {
        case RESTART_ALWAYS:
            should_restart = true;
            break;
        case RESTART_ON_FAILURE:
            should_restart = (found->exit_code != 0);
            break;
        case RESTART_ON_ABNORMAL:
            should_restart = (found->exit_code < 0); /* killed by signal / 被信号杀死 */
            break;
        case RESTART_NEVER:
        default:
            break;
        }

        if (should_restart &&
            found->restart_count < found->restart_max_retries) {
            found->state = SVC_STATE_RESTARTING;
            found->restart_count++;
            LOG_I("scheduling restart for '%s' (attempt %d/%d, delay %dms)",
                  found->name, found->restart_count,
                  found->restart_max_retries, found->restart_delay_ms);
        } else if (should_restart) {
            LOG_E("service '%s' exceeded max restart attempts (%d), marking failed",
                  found->name, found->restart_max_retries);
            found->state = SVC_STATE_FAILED;
        } else {
            found->state = SVC_STATE_STOPPED;
        }
    }
}

/*
 * Periodic service health check.
 * 定期服务健康检查。
 *
 * Called from main event loop. Handles deferred restarts.
 * 从主事件循环调用，处理延迟重启。
 */
void supervisor_check_services(void)
{
    uint64_t now = util_monotonic_ns();

    /* Only check every CHECK_INTERVAL / 每隔 CHECK_INTERVAL 检查一次 */
    if (now - g_last_check_time < CHECK_INTERVAL_NS)
        return;
    g_last_check_time = now;

    service_t *svc = service_get_list();
    while (svc) {
        if (svc->state == SVC_STATE_RESTARTING) {
            /* Check if restart delay has elapsed / 检查重启延迟是否已过 */
            uint64_t elapsed_ms =
                (now - svc->stop_time) / 1000000ULL;

            if (elapsed_ms >= (uint64_t)svc->restart_delay_ms) {
                LOG_I("restarting service '%s' (attempt %d)",
                      svc->name, svc->restart_count);
                if (service_start(svc) < 0) {
                    LOG_E("restart failed for service '%s'", svc->name);
                    svc->state = SVC_STATE_FAILED;
                }
            }
        }

        svc = svc->next;
    }
}

/*
 * Main supervisor run loop (unused - integrated into event_loop).
 * 监控器主循环 (未使用 - 已集成到 event_loop)。
 */
void supervisor_run(void)
{
    /* Placeholder - logic is in supervisor_check_services() */
    /* 占位 - 逻辑在 supervisor_check_services() 中 */
}
