/*
 * main.c - Android container entry point / Android 容器入口点
 *
 * CLI tool to start/stop/manage the Android compatibility container.
 * 启动/停止/管理 Android 兼容容器的命令行工具。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "container.h"

static container_t g_container = {0};

static void handle_signal(int sig)
{
    (void)sig;
    fprintf(stderr, "[android-container] received signal, stopping...\n");
    container_stop(&g_container);
    exit(0);
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "android-container %s - LumiOS Android Compatibility Layer\n"
        "\n"
        "Usage: %s <command> [options]\n"
        "\n"
        "Commands:\n"
        "  start       Start the Android container\n"
        "  stop        Stop the running container\n"
        "  status      Show container status\n"
        "\n"
        "Options:\n"
        "  --share-net     Share host network namespace\n"
        "  --gpu           Enable GPU passthrough\n"
        "  --memory <MB>   Memory limit in MB (default: unlimited)\n"
        "  --help          Show this help\n"
        "  --version       Show version\n",
        CONTAINER_VERSION, prog);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        usage(argv[0]);
        return 0;
    }
    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        printf("android-container %s\n", CONTAINER_VERSION);
        return 0;
    }

    /* Parse options / 解析选项 */
    ct_config_t config = {0};
    config.ns_flags = NS_ALL;
    config.memory_limit = -1;
    config.cpu_shares = 100;
    strncpy(config.hostname, "android", sizeof(config.hostname) - 1);

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--share-net") == 0) {
            config.share_network = true;
        } else if (strcmp(argv[i], "--gpu") == 0) {
            config.gpu_passthrough = true;
        } else if (strcmp(argv[i], "--memory") == 0 && i + 1 < argc) {
            config.memory_limit = atoll(argv[++i]) * 1024 * 1024;
        }
    }

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    if (strcmp(cmd, "start") == 0) {
        /* Check if already running / 检查是否已运行 */
        if (access(CONTAINER_PID_FILE, F_OK) == 0) {
            fprintf(stderr, "ERROR: container already running (pid file exists)\n");
            fprintf(stderr, "  Run 'android-container stop' first\n");
            return 1;
        }

        container_init(&g_container, &config);

        fprintf(stderr, "[android-container] starting with options:\n");
        fprintf(stderr, "  Network: %s\n", config.share_network ? "shared" : "isolated");
        fprintf(stderr, "  GPU:     %s\n", config.gpu_passthrough ? "enabled" : "disabled");
        if (config.memory_limit > 0) {
            fprintf(stderr, "  Memory:  %ldMB\n", (long)(config.memory_limit / 1024 / 1024));
        }

        if (container_start(&g_container) < 0) {
            fprintf(stderr, "ERROR: failed to start container\n");
            return 1;
        }

        /* Wait for container to exit / 等待容器退出 */
        int ret = container_wait(&g_container);
        fprintf(stderr, "[android-container] container exited with code %d\n", ret);
        return ret;

    } else if (strcmp(cmd, "stop") == 0) {
        /* Read PID and send SIGTERM / 读取 PID 并发送 SIGTERM */
        FILE *fp = fopen(CONTAINER_PID_FILE, "r");
        if (!fp) {
            fprintf(stderr, "Container is not running\n");
            return 0;
        }
        int pid;
        if (fscanf(fp, "%d", &pid) == 1 && pid > 0) {
            fprintf(stderr, "Stopping container (pid %d)...\n", pid);
            kill(pid, SIGTERM);
        }
        fclose(fp);
        return 0;

    } else if (strcmp(cmd, "status") == 0) {
        if (access(CONTAINER_PID_FILE, F_OK) == 0) {
            FILE *fp = fopen(CONTAINER_PID_FILE, "r");
            int pid = 0;
            if (fp) { fscanf(fp, "%d", &pid); fclose(fp); }
            printf("Android container: running (pid %d)\n", pid);
        } else {
            printf("Android container: stopped\n");
        }
        return 0;

    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        usage(argv[0]);
        return 1;
    }
}
