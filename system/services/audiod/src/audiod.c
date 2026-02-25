/*
 * audiod.c - Core audio daemon / 核心音频守护进程
 *
 * Initializes PipeWire, manages audio routing and volume.
 * 初始化 PipeWire，管理音频路由和音量。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "audiod.h"

int audiod_init(void)
{
    fprintf(stderr, "[audiod] initializing audio subsystems\n");

    if (volume_init() < 0) {
        fprintf(stderr, "[audiod] WARNING: volume init failed\n");
    }

    if (pw_init() < 0) {
        fprintf(stderr, "[audiod] WARNING: PipeWire init failed\n");
    }

    fprintf(stderr, "[audiod] initialization complete\n");
    return 0;
}

void audiod_run(void)
{
    fprintf(stderr, "[audiod] entering event loop\n");
    /* TODO: PipeWire main loop + IPC socket / TODO: PipeWire 主循环 + IPC 套接字 */
    while (1) { sleep(5); }
}

void audiod_shutdown(void)
{
    fprintf(stderr, "[audiod] shutting down\n");
    volume_save();
    pw_shutdown();
}
