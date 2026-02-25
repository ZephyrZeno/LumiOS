/*
 * lumid - LumiOS Init System (PID 1)
 *
 * Boot sequence / 启动流程:
 * 1. Mount base filesystems (proc, sys, dev, run, tmp) / 挂载基础文件系统
 * 2. Initialize cgroups v2 / 初始化 cgroups v2
 * 3. Load service configurations / 加载服务配置
 * 4. Resolve dependencies (topological sort) / 解析依赖关系 (拓扑排序)
 * 5. Start services in order / 按序启动服务
 * 6. Enter main loop (supervise services + handle IPC) / 进入主循环
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/signalfd.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <linux/reboot.h>

#include "lumid.h"

/* Global state / 全局状态 */
static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_reboot  = 0;
static int g_epoll_fd  = -1;
static int g_signal_fd = -1;
static int g_server_fd = -1;

#define MAX_EPOLL_EVENTS 16

/* --- Signal handling / 信号处理 --- */
static int setup_signals(void)
{
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGHUP);

    /* Block these signals, handle via signalfd / 阻塞信号，通过 signalfd 处理 */
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        LOG_E("sigprocmask failed: %s", strerror(errno));
        return -1;
    }

    g_signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (g_signal_fd < 0) {
        LOG_E("signalfd failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

static void handle_signal(void)
{
    struct signalfd_siginfo si;
    ssize_t n;

    while ((n = read(g_signal_fd, &si, sizeof(si))) == sizeof(si)) {
        switch (si.ssi_signo) {
        case SIGCHLD:
            supervisor_handle_sigchld();
            break;

        case SIGTERM:
        case SIGINT:
            LOG_I("received shutdown signal (%d)", si.ssi_signo);
            g_running = 0;
            break;

        case SIGHUP:
            LOG_I("received SIGHUP, reloading service configs");
            service_load_all(LUMID_SERVICE_DIR);
            service_resolve_deps();
            break;
        }
    }
}

/* --- Early system init / 系统早期初始化 --- */
static int early_init(void)
{
    LOG_I("LumiOS lumid v%s starting", LUMID_VERSION_STRING);
    LOG_I("PID: %d", getpid());

    /* Set hostname / 设置主机名 */
    if (sethostname("lumios", 6) < 0) {
        LOG_W("failed to set hostname: %s", strerror(errno));
    }

    /* Mount base filesystems / 挂载基础文件系统 */
    if (mount_initial_filesystems() < 0) {
        LOG_E("failed to mount initial filesystems");
        return -1;
    }

    /* Create runtime directories / 创建运行时目录 */
    util_mkdir_p(LUMID_RUNTIME_DIR, 0755);
    util_mkdir_p(LUMID_LOG_DIR, 0755);
    util_mkdir_p("/run/user", 0755);
    util_mkdir_p("/tmp", 01777);

    /* Initialize cgroups / 初始化 cgroups */
    if (cgroup_init() < 0) {
        LOG_W("cgroup init failed, resource limits disabled");
    }

    /* Write PID file / 写入 PID 文件 */
    util_write_file(LUMID_PID_FILE, "1\n");

    return 0;
}

/* --- Load and start services / 加载并启动服务 --- */
static int load_and_start_services(void)
{
    int count;

    LOG_I("loading service configs from %s", LUMID_SERVICE_DIR);

    if (service_load_all(LUMID_SERVICE_DIR) < 0) {
        LOG_E("failed to load service configs");
        return -1;
    }

    count = service_get_count();
    LOG_I("loaded %d services", count);

    if (service_resolve_deps() < 0) {
        LOG_E("dependency resolution failed");
        return -1;
    }

    LOG_I("starting services...");
    if (service_start_all() < 0) {
        LOG_W("some services failed to start");
    }

    return 0;
}

/* --- Main event loop / 主事件循环 --- */
static int event_loop(void)
{
    struct epoll_event events[MAX_EPOLL_EVENTS];
    struct epoll_event ev;

    g_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (g_epoll_fd < 0) {
        LOG_F("epoll_create1 failed: %s", strerror(errno));
        return -1;
    }

    /* Register signalfd / 注册信号文件描述符 */
    ev.events = EPOLLIN;
    ev.data.fd = g_signal_fd;
    if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, g_signal_fd, &ev) < 0) {
        LOG_F("failed to register signalfd: %s", strerror(errno));
        return -1;
    }

    /* Start IPC server / 启动 IPC 服务器 */
    g_server_fd = socket_server_init(LUMID_SOCKET_PATH);
    if (g_server_fd >= 0) {
        ev.events = EPOLLIN;
        ev.data.fd = g_server_fd;
        epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, g_server_fd, &ev);
        LOG_I("IPC server ready: %s", LUMID_SOCKET_PATH);
    }

    LOG_I("entering main event loop");

    while (g_running) {
        int nfds = epoll_wait(g_epoll_fd, events, MAX_EPOLL_EVENTS, 5000);

        if (nfds < 0) {
            if (errno == EINTR)
                continue;
            LOG_E("epoll_wait failed: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == g_signal_fd) {
                handle_signal();
            } else if (fd == g_server_fd) {
                /* Accept new IPC connection / 接受新的 IPC 连接 */
                int client_fd = socket_server_accept(g_server_fd);
                if (client_fd >= 0) {
                    socket_handle_request(client_fd);
                    close(client_fd);
                }
            }
        }

        /* Periodic service health check / 定期检查服务状态 */
        supervisor_check_services();
    }

    return 0;
}

/* --- System shutdown / 系统关机 --- */
static void shutdown_system(void)
{
    LOG_I("initiating system shutdown...");

    /* Stop all services (reverse dependency order) / 按反向依赖顺序停止所有服务 */
    service_stop_all();

    /* Sync filesystems / 同步文件系统 */
    sync();

    /* Close logging / 关闭日志 */
    log_close();

    /* Only call reboot() when running as PID 1 (real init) */
    /* 仅在 PID 1（真实 init）时调用 reboot() */
    if (getpid() == 1) {
        if (g_reboot) {
            LOG_I("rebooting...");
            reboot(LINUX_REBOOT_CMD_RESTART);
        } else {
            LOG_I("powering off...");
            reboot(LINUX_REBOOT_CMD_POWER_OFF);
        }
    } else {
        LOG_I("test mode exit (not calling reboot)");
    }
}

/* --- Entry point / 入口点 --- */
int main(int argc, char *argv[])
{
    /* Initialize logging / 初始化日志 */
    log_init(LUMID_LOG_DIR);
    log_set_level(LOG_INFO);

    /* Check if running as PID 1 / 检查是否作为 PID 1 运行 */
    if (getpid() != 1) {
        /* Non-PID 1 mode - possibly for testing / 非 PID 1 模式，用于测试 */
        if (argc > 1 && strcmp(argv[1], "--test") == 0) {
            LOG_I("test mode (PID %d)", getpid());
        } else {
            fprintf(stderr, "lumid: must run as PID 1\n");
            fprintf(stderr, "usage: lumid [--test]\n");
            return 1;
        }
    }

    /* Process command line arguments / 处理命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("lumid %s\n", LUMID_VERSION_STRING);
            return 0;
        } else if (strcmp(argv[i], "--debug") == 0) {
            log_set_level(LOG_DEBUG);
        } else if (strcmp(argv[i], "--reboot") == 0) {
            g_reboot = 1;
        }
    }

    /* Early init / 早期初始化 */
    if (early_init() < 0) {
        LOG_F("early init failed, dropping to emergency shell");
        execl("/bin/sh", "/bin/sh", "-i", NULL);
        return 1;
    }

    /* Setup signal handling / 设置信号处理 */
    if (setup_signals() < 0) {
        LOG_F("signal setup failed");
        return 1;
    }

    /* Initialize process supervisor / 初始化进程监控 */
    if (supervisor_init() < 0) {
        LOG_F("supervisor init failed");
        return 1;
    }

    /* Load and start services / 加载并启动服务 */
    load_and_start_services();

    /* Main event loop / 主事件循环 */
    event_loop();

    /* Cleanup and shutdown / 清理并关机 */
    shutdown_system();

    return 0;
}
