/*
 * main.c - Sensor daemon entry point / 传感器守护进程入口
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "sensord.h"

static sensord_t sd;

static void signal_handler(int sig)
{
    (void)sig;
    sd.running = false;
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (sensord_init(&sd) < 0) {
        fprintf(stderr, "[sensord] init failed\n");
        return 1;
    }

    fprintf(stderr, "[sensord] running\n");
    sensord_run(&sd);
    sensord_shutdown(&sd);
    return 0;
}
