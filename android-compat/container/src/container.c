/*
 * container.c - Android container lifecycle / Android 容器生命周期
 *
 * Creates and manages the containerized Android runtime.
 * Uses clone() with namespace flags to isolate the Android environment.
 *
 * 创建和管理容器化的 Android 运行时。
 * 使用 clone() 配合命名空间标志隔离 Android 环境。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include "container.h"

/* Container child stack size / 容器子进程栈大小 */
#define STACK_SIZE (1024 * 1024)

const char *container_state_str(ct_state_t state)
{
    switch (state) {
    case CT_STOPPED:  return "stopped";
    case CT_STARTING: return "starting";
    case CT_RUNNING:  return "running";
    case CT_STOPPING: return "stopping";
    case CT_FAILED:   return "failed";
    default:          return "unknown";
    }
}

/*
 * Container init process entry point (runs inside namespaces).
 * 容器 init 进程入口（在命名空间内运行）。
 */
static int container_child(void *arg)
{
    container_t *ct = (container_t *)arg;

    /* Signal parent that we're alive / 通知父进程我们已启动 */
    close(ct->status_pipe[0]);
    write(ct->status_pipe[1], "OK", 2);
    close(ct->status_pipe[1]);

    /* Setup mount namespace / 设置挂载命名空间 */
    if (ns_setup_mount() < 0) {
        fprintf(stderr, "ERROR: mount namespace setup failed\n");
        return 1;
    }

    /* Setup Android root filesystem / 设置 Android 根文件系统 */
    if (mounts_setup_android_rootfs() < 0) {
        fprintf(stderr, "ERROR: Android rootfs setup failed\n");
        return 1;
    }

    /* Bind device nodes needed by Android / 绑定 Android 需要的设备节点 */
    if (mounts_bind_device_nodes() < 0) {
        fprintf(stderr, "WARNING: some device nodes failed to bind\n");
    }

    /* Setup shared storage (/sdcard) / 设置共享存储 */
    mounts_setup_shared_storage();

    /* GPU passthrough if enabled / GPU 直通（如果启用） */
    if (ct->config.gpu_passthrough) {
        mounts_setup_gpu_access();
    }

    /* Setup UTS namespace (hostname) / 设置 UTS 命名空间（主机名） */
    if (ct->config.hostname[0] != '\0') {
        ns_setup_uts(ct->config.hostname);
    }

    /* Load Android system properties / 加载 Android 系统属性 */
    props_init();
    props_load_defaults();

    fprintf(stderr, "[android-container] container init ready, starting Android runtime\n");

    /* Pivot root into Android filesystem / 切换根到 Android 文件系统 */
    if (chdir(ANDROID_ROOT) < 0) {
        fprintf(stderr, "ERROR: chdir to %s failed: %s\n",
                ANDROID_ROOT, strerror(errno));
        return 1;
    }

    /*
     * Execute Android init (or a shim that bootstraps ART).
     * 执行 Android init（或引导 ART 的垫片）。
     */
    char *init_path = "/system/bin/init";
    char *argv[] = { "init", NULL };
    char *envp[] = {
        "PATH=/system/bin:/system/xbin:/vendor/bin",
        "ANDROID_ROOT=/system",
        "ANDROID_DATA=/data",
        "ANDROID_RUNTIME_ROOT=/apex/com.android.runtime",
        NULL,
    };

    execve(init_path, argv, envp);
    fprintf(stderr, "ERROR: execve %s failed: %s\n", init_path, strerror(errno));
    return 1;
}

/* === Initialize container / 初始化容器 === */

int container_init(container_t *ct, const ct_config_t *config)
{
    memset(ct, 0, sizeof(*ct));
    ct->config = *config;
    ct->state = CT_STOPPED;
    ct->init_pid = -1;

    /* Set defaults / 设置默认值 */
    if (ct->config.ns_flags == 0)
        ct->config.ns_flags = NS_ALL;
    if (ct->config.hostname[0] == '\0')
        strncpy(ct->config.hostname, "android", sizeof(ct->config.hostname) - 1);

    return 0;
}

/* === Start container / 启动容器 === */

int container_start(container_t *ct)
{
    if (ct->state == CT_RUNNING) {
        fprintf(stderr, "WARNING: container already running (pid %d)\n", ct->init_pid);
        return 0;
    }

    fprintf(stderr, "[android-container] starting (ns_flags=0x%x)\n", ct->config.ns_flags);
    ct->state = CT_STARTING;

    /* Create status pipe / 创建状态管道 */
    if (pipe2(ct->status_pipe, O_CLOEXEC) < 0) {
        fprintf(stderr, "ERROR: pipe2 failed: %s\n", strerror(errno));
        ct->state = CT_FAILED;
        return -1;
    }

    /* Create cgroup for container / 为容器创建 cgroup */
    ct_cgroup_create("android");
    if (ct->config.memory_limit > 0 || ct->config.cpu_shares > 0) {
        ct_cgroup_set_limits("android", ct->config.memory_limit,
                             ct->config.cpu_shares);
    }

    /* Map namespace flags to clone flags / 将命名空间标志映射到 clone 标志 */
    int clone_flags = SIGCHLD;
    if (ct->config.ns_flags & NS_MOUNT)  clone_flags |= CLONE_NEWNS;
    if (ct->config.ns_flags & NS_PID)    clone_flags |= CLONE_NEWPID;
    if (ct->config.ns_flags & NS_IPC)    clone_flags |= CLONE_NEWIPC;
    if (ct->config.ns_flags & NS_UTS)    clone_flags |= CLONE_NEWUTS;
    if (ct->config.ns_flags & NS_CGROUP) clone_flags |= CLONE_NEWCGROUP;
    if (!ct->config.share_network && (ct->config.ns_flags & NS_NET))
        clone_flags |= CLONE_NEWNET;

    /* Allocate child stack / 分配子进程栈 */
    char *stack = malloc(STACK_SIZE);
    if (!stack) {
        fprintf(stderr, "ERROR: stack allocation failed\n");
        ct->state = CT_FAILED;
        return -1;
    }
    char *stack_top = stack + STACK_SIZE;

    /* Clone into new namespaces / 克隆到新命名空间 */
    pid_t pid = clone(container_child, stack_top, clone_flags, ct);
    if (pid < 0) {
        fprintf(stderr, "ERROR: clone failed: %s\n", strerror(errno));
        free(stack);
        ct->state = CT_FAILED;
        return -1;
    }

    ct->init_pid = pid;
    free(stack);

    /* Add container init to cgroup / 将容器 init 添加到 cgroup */
    ct_cgroup_add_pid("android", pid);

    /* Wait for child to signal ready / 等待子进程就绪信号 */
    close(ct->status_pipe[1]);
    char buf[4] = {0};
    read(ct->status_pipe[0], buf, sizeof(buf));
    close(ct->status_pipe[0]);

    if (strncmp(buf, "OK", 2) == 0) {
        ct->state = CT_RUNNING;
        fprintf(stderr, "[android-container] running (pid %d)\n", pid);

        /* Write PID file / 写入 PID 文件 */
        char pid_str[16];
        snprintf(pid_str, sizeof(pid_str), "%d\n", pid);
        int fd = open(CONTAINER_PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { write(fd, pid_str, strlen(pid_str)); close(fd); }
    } else {
        ct->state = CT_FAILED;
        fprintf(stderr, "ERROR: container init failed to start\n");
        return -1;
    }

    return 0;
}

/* === Stop container / 停止容器 === */

int container_stop(container_t *ct)
{
    if (ct->state != CT_RUNNING || ct->init_pid <= 0)
        return 0;

    fprintf(stderr, "[android-container] stopping (pid %d)\n", ct->init_pid);
    ct->state = CT_STOPPING;

    /* Send SIGTERM, wait, then SIGKILL / 发送 SIGTERM，等待，然后 SIGKILL */
    kill(ct->init_pid, SIGTERM);

    int status;
    pid_t ret = waitpid(ct->init_pid, &status, WNOHANG);
    if (ret == 0) {
        usleep(3000000); /* 3 seconds grace / 3 秒宽限期 */
        ret = waitpid(ct->init_pid, &status, WNOHANG);
        if (ret == 0) {
            fprintf(stderr, "WARNING: container did not stop gracefully, sending SIGKILL\n");
            kill(ct->init_pid, SIGKILL);
            waitpid(ct->init_pid, &status, 0);
        }
    }

    ct->state = CT_STOPPED;
    ct->init_pid = -1;
    unlink(CONTAINER_PID_FILE);

    /* Cleanup cgroup / 清理 cgroup */
    ct_cgroup_destroy("android");

    fprintf(stderr, "[android-container] stopped\n");
    return 0;
}

/* === Wait for container to exit / 等待容器退出 === */

int container_wait(container_t *ct)
{
    if (ct->init_pid <= 0) return 0;

    int status;
    waitpid(ct->init_pid, &status, 0);

    ct->state = CT_STOPPED;
    ct->init_pid = -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* === Destroy container / 销毁容器 === */

void container_destroy(container_t *ct)
{
    if (ct->state == CT_RUNNING)
        container_stop(ct);
}
