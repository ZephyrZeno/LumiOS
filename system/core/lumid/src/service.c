/*
 * service.c - Service lifecycle management / 服务生命周期管理
 *
 * Handles loading, starting, stopping, restarting services.
 * 负责服务的加载、启动、停止、重启。
 *
 * Dependency resolution via topological sort.
 * 通过拓扑排序进行依赖解析。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#include "lumid.h"

/* Global service linked list / 全局服务链表 */
static service_t *g_services = NULL;
static int g_service_count = 0;

/* Start order after topological sort / 拓扑排序后的启动顺序 */
static service_t *g_start_order[LUMID_MAX_SERVICES];
static int g_start_order_count = 0;

/* === Service creation/destruction / 服务创建与销毁 === */

service_t *service_create(void)
{
    service_t *svc = calloc(1, sizeof(service_t));
    if (!svc)
        return NULL;

    svc->state = SVC_STATE_STOPPED;
    svc->pid = -1;
    svc->exit_code = -1;
    svc->log_fd = -1;
    svc->restart_policy = RESTART_NEVER;
    svc->restart_delay_ms = 1000;
    svc->restart_max_retries = 5;
    svc->type = SVC_TYPE_SIMPLE;
    svc->enabled = true;

    /* Default cgroup limits (unlimited) / 默认 cgroup 限制 (无限) */
    svc->cgroup.memory_max = -1;
    svc->cgroup.memory_high = -1;
    svc->cgroup.cpu_weight = 100;
    svc->cgroup.cpu_max = -1;
    svc->cgroup.io_weight = 100;
    svc->cgroup.pids_max = -1;

    return svc;
}

void service_free(service_t *svc)
{
    if (!svc)
        return;
    if (svc->log_fd >= 0)
        close(svc->log_fd);
    free(svc);
}

/* === Service lookup / 服务查找 === */

service_t *service_find(const char *name)
{
    service_t *svc = g_services;
    while (svc) {
        if (strcmp(svc->name, name) == 0)
            return svc;
        svc = svc->next;
    }
    return NULL;
}

int service_get_count(void)
{
    return g_service_count;
}

service_t *service_get_list(void)
{
    return g_services;
}

const char *service_state_str(svc_state_t state)
{
    switch (state) {
    case SVC_STATE_STOPPED:    return "stopped";
    case SVC_STATE_STARTING:   return "starting";
    case SVC_STATE_RUNNING:    return "running";
    case SVC_STATE_STOPPING:   return "stopping";
    case SVC_STATE_FAILED:     return "failed";
    case SVC_STATE_RESTARTING: return "restarting";
    default:                   return "unknown";
    }
}

/* === Service registration / 服务注册 === */

static void service_register(service_t *svc)
{
    service_t *existing = service_find(svc->name);
    if (existing) {
        LOG_W("service '%s' already registered, skipping", svc->name);
        service_free(svc);
        return;
    }

    svc->next = g_services;
    g_services = svc;
    g_service_count++;

    LOG_D("registered service: %s (%s)", svc->name, svc->exec_path);
}

/* === Load all services from directory / 从目录加载所有服务 === */

int service_load_all(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *entry;
    char path[LUMID_MAX_PATH_LEN];
    int loaded = 0;

    if (!d) {
        LOG_E("failed to open service dir: %s: %s", dir, strerror(errno));
        return -1;
    }

    while ((entry = readdir(d)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);

        /* Only process .svc files / 只处理 .svc 文件 */
        if (len < 5 || strcmp(name + len - 4, ".svc") != 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir, name);

        service_t *svc = service_create();
        if (!svc) {
            LOG_E("memory allocation failed");
            continue;
        }

        if (config_parse_service(path, svc) < 0) {
            LOG_E("failed to parse service config: %s", path);
            service_free(svc);
            continue;
        }

        strncpy(svc->config_path, path, LUMID_MAX_PATH_LEN - 1);
        service_register(svc);
        loaded++;
    }

    closedir(d);
    LOG_I("loaded %d service configs from %s", loaded, dir);
    return loaded;
}

/* === Topological sort for dependency resolution / 拓扑排序解析依赖 === */

/* Visited state for DFS / DFS 访问状态 */
#define VISIT_NONE     0
#define VISIT_TEMP     1  /* Currently in recursion / 递归中 */
#define VISIT_PERM     2  /* Fully processed / 已处理 */

static int visit_state[LUMID_MAX_SERVICES];

/*
 * Map service name to index in flat array for sorting.
 * 将服务名映射到平面数组索引用于排序。
 */
static service_t *svc_array[LUMID_MAX_SERVICES];
static int svc_array_count = 0;

static int svc_index(const char *name)
{
    for (int i = 0; i < svc_array_count; i++) {
        if (strcmp(svc_array[i]->name, name) == 0)
            return i;
    }
    return -1;
}

/*
 * Recursive DFS visit for topological sort.
 * 拓扑排序的递归 DFS 访问。
 */
static int topo_visit(int idx)
{
    if (visit_state[idx] == VISIT_PERM)
        return 0;
    if (visit_state[idx] == VISIT_TEMP) {
        LOG_E("circular dependency detected involving '%s'",
              svc_array[idx]->name);
        return -1;
    }

    visit_state[idx] = VISIT_TEMP;
    service_t *svc = svc_array[idx];

    /* Visit all 'after' dependencies first / 先访问所有 after 依赖 */
    for (int i = 0; i < svc->after_count; i++) {
        int dep = svc_index(svc->after[i]);
        if (dep >= 0) {
            if (topo_visit(dep) < 0)
                return -1;
        }
    }

    /* Visit all 'requires' dependencies / 访问所有 requires 依赖 */
    for (int i = 0; i < svc->requires_count; i++) {
        int dep = svc_index(svc->requires[i]);
        if (dep >= 0) {
            if (topo_visit(dep) < 0)
                return -1;
        }
    }

    visit_state[idx] = VISIT_PERM;
    g_start_order[g_start_order_count++] = svc;
    return 0;
}

int service_resolve_deps(void)
{
    /* Build flat array from linked list / 从链表构建平面数组 */
    svc_array_count = 0;
    service_t *svc = g_services;
    while (svc && svc_array_count < LUMID_MAX_SERVICES) {
        svc_array[svc_array_count++] = svc;
        svc = svc->next;
    }

    memset(visit_state, 0, sizeof(visit_state));
    g_start_order_count = 0;

    for (int i = 0; i < svc_array_count; i++) {
        if (visit_state[i] == VISIT_NONE) {
            if (topo_visit(i) < 0)
                return -1;
        }
    }

    LOG_I("dependency resolution complete, start order (%d services):",
          g_start_order_count);
    for (int i = 0; i < g_start_order_count; i++) {
        LOG_D("  [%d] %s", i, g_start_order[i]->name);
    }

    return 0;
}

/* === Start a single service / 启动单个服务 === */

int service_start(service_t *svc)
{
    if (!svc)
        return -1;

    if (svc->state == SVC_STATE_RUNNING) {
        LOG_W("service '%s' is already running (pid %d)",
              svc->name, svc->pid);
        return 0;
    }

    LOG_I("starting service '%s': %s", svc->name, svc->exec_path);
    svc->state = SVC_STATE_STARTING;

    /* Check required dependencies are running / 检查必需依赖是否运行中 */
    for (int i = 0; i < svc->requires_count; i++) {
        service_t *dep = service_find(svc->requires[i]);
        if (dep && dep->state != SVC_STATE_RUNNING) {
            LOG_E("dependency '%s' not running for service '%s'",
                  svc->requires[i], svc->name);
            svc->state = SVC_STATE_FAILED;
            return -1;
        }
    }

    /* Open log file / 打开日志文件 */
    if (svc->log_fd < 0) {
        snprintf(svc->log_path, sizeof(svc->log_path),
                 "%s/%s.log", LUMID_LOG_DIR, svc->name);
        svc->log_fd = open(svc->log_path,
                           O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    }

    /* Fork and exec / 创建子进程并执行 */
    pid_t pid = fork();
    if (pid < 0) {
        LOG_E("fork failed for service '%s': %s", svc->name, strerror(errno));
        svc->state = SVC_STATE_FAILED;
        return -1;
    }

    if (pid == 0) {
        /* === Child process / 子进程 === */

        /* Redirect stdout/stderr to log / 重定向标准输出到日志 */
        if (svc->log_fd >= 0) {
            dup2(svc->log_fd, STDOUT_FILENO);
            dup2(svc->log_fd, STDERR_FILENO);
        }

        /* Set environment variables / 设置环境变量 */
        for (int i = 0; i < svc->env_count; i++) {
            setenv(svc->env[i].key, svc->env[i].value, 1);
        }

        /* Change working directory / 切换工作目录 */
        if (svc->working_dir[0] != '\0') {
            if (chdir(svc->working_dir) < 0) {
                fprintf(stderr, "chdir failed: %s\n", strerror(errno));
                _exit(127);
            }
        }

        /* Drop privileges if user specified / 如果指定了用户则降权 */
        if (svc->user[0] != '\0') {
            util_drop_privileges(svc->user, svc->group);
        }

        /* Create new session / 创建新会话 */
        setsid();

        /* Execute service binary / 执行服务二进制 */
        if (svc->exec_args[0] != '\0') {
            execl("/bin/sh", "sh", "-c", svc->exec_args, NULL);
        } else {
            execl(svc->exec_path, svc->exec_path, NULL);
        }

        /* If we get here, exec failed / 到这里说明 exec 失败 */
        fprintf(stderr, "exec failed: %s\n", strerror(errno));
        _exit(127);
    }

    /* === Parent process / 父进程 === */
    svc->pid = pid;
    svc->start_time = util_monotonic_ns();
    svc->restart_count = 0;

    /* Create cgroup and add PID / 创建 cgroup 并添加 PID */
    if (svc->cgroup.memory_max > 0 || svc->cgroup.cpu_max > 0) {
        cgroup_create(svc->name);
        cgroup_apply_limits(svc->name, &svc->cgroup);
        cgroup_add_pid(svc->name, pid);
    }

    /* For simple services, consider running immediately / 简单服务立即标记为运行中 */
    if (svc->type == SVC_TYPE_SIMPLE) {
        svc->state = SVC_STATE_RUNNING;
    }

    LOG_I("service '%s' started (pid %d)", svc->name, pid);
    return 0;
}

/* === Stop a single service / 停止单个服务 === */

int service_stop(service_t *svc)
{
    if (!svc)
        return -1;

    if (svc->state != SVC_STATE_RUNNING &&
        svc->state != SVC_STATE_STARTING) {
        return 0;
    }

    LOG_I("stopping service '%s' (pid %d)", svc->name, svc->pid);
    svc->state = SVC_STATE_STOPPING;

    /* Send SIGTERM first / 先发送 SIGTERM */
    if (svc->pid > 0) {
        kill(svc->pid, SIGTERM);
    }

    /* Wait briefly, then SIGKILL if needed / 短暂等待后如有需要发送 SIGKILL */
    usleep(500000); /* 500ms */

    if (svc->pid > 0) {
        int status;
        pid_t ret = waitpid(svc->pid, &status, WNOHANG);
        if (ret == 0) {
            /* Still alive, force kill / 仍存活，强制杀死 */
            LOG_W("service '%s' did not stop gracefully, sending SIGKILL",
                  svc->name);
            kill(svc->pid, SIGKILL);
            waitpid(svc->pid, &status, 0);
        }

        if (WIFEXITED(status)) {
            svc->exit_code = WEXITSTATUS(status);
        }
    }

    svc->state = SVC_STATE_STOPPED;
    svc->stop_time = util_monotonic_ns();
    svc->pid = -1;

    /* Remove cgroup / 移除 cgroup */
    cgroup_remove(svc->name);

    LOG_I("service '%s' stopped (exit code %d)", svc->name, svc->exit_code);
    return 0;
}

/* === Restart a service / 重启服务 === */

int service_restart(service_t *svc)
{
    if (!svc)
        return -1;

    LOG_I("restarting service '%s'", svc->name);
    svc->state = SVC_STATE_RESTARTING;

    service_stop(svc);

    /* Apply restart delay / 应用重启延迟 */
    if (svc->restart_delay_ms > 0) {
        usleep((useconds_t)svc->restart_delay_ms * 1000);
    }

    return service_start(svc);
}

/* === Start all services in dependency order / 按依赖顺序启动所有服务 === */

int service_start_all(void)
{
    int failed = 0;

    for (int i = 0; i < g_start_order_count; i++) {
        service_t *svc = g_start_order[i];
        if (!svc->enabled) {
            LOG_D("service '%s' is disabled, skipping", svc->name);
            continue;
        }
        if (service_start(svc) < 0) {
            failed++;
        }
    }

    if (failed > 0) {
        LOG_W("%d services failed to start", failed);
        return -1;
    }

    return 0;
}

/* === Stop all services in reverse order / 按反向顺序停止所有服务 === */

void service_stop_all(void)
{
    LOG_I("stopping all services...");

    /* Stop in reverse start order / 按启动顺序的反向停止 */
    for (int i = g_start_order_count - 1; i >= 0; i--) {
        service_stop(g_start_order[i]);
    }
}
