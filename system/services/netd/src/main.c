/*
 * main.c - Network Manager Entry / 网络管理服务入口
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "netd.h"

static volatile int g_running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("netd %s\n", NETD_VERSION);
            return 0;
        }
    }

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    fprintf(stderr, "[netd] v%s starting\n", NETD_VERSION);

    if (netd_init() < 0) {
        fprintf(stderr, "[netd] ERROR: initialization failed\n");
        return 1;
    }

    fprintf(stderr, "[netd] ready\n");
    netd_run();

    netd_shutdown();
    fprintf(stderr, "[netd] shutdown complete\n");
    return 0;
}
