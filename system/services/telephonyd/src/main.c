/*
 * main.c - Telephony daemon entry point / 电话守护进程入口
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "telephonyd.h"

static telephonyd_t td;

static void signal_handler(int sig)
{
    (void)sig;
    td.running = false;
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (telephonyd_init(&td) < 0) {
        fprintf(stderr, "[telephonyd] init failed\n");
        return 1;
    }

    fprintf(stderr, "[telephonyd] running\n");
    telephonyd_run(&td);
    telephonyd_shutdown(&td);
    return 0;
}
