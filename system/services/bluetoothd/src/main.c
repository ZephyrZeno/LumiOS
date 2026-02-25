/*
 * main.c - Bluetooth daemon entry point / 蓝牙守护进程入口
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "bluetoothd.h"

static bluetoothd_t bt;
static volatile int running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (btd_init(&bt) < 0) {
        fprintf(stderr, "[bluetoothd] init failed\n");
        return 1;
    }

    fprintf(stderr, "[bluetoothd] running\n");
    btd_run(&bt);
    btd_shutdown(&bt);
    return 0;
}
