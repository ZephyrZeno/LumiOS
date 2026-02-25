/*
 * main.c - Audio Service Entry / 音频服务入口
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "audiod.h"

static volatile int g_running = 1;
static void handle_signal(int sig) { (void)sig; g_running = 0; }

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("audiod %s\n", AUDIOD_VERSION);
            return 0;
        }
    }

    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    fprintf(stderr, "[audiod] v%s starting\n", AUDIOD_VERSION);

    if (audiod_init() < 0) {
        fprintf(stderr, "[audiod] ERROR: initialization failed\n");
        return 1;
    }

    fprintf(stderr, "[audiod] ready\n");
    audiod_run();
    audiod_shutdown();

    fprintf(stderr, "[audiod] shutdown complete\n");
    return 0;
}
