/*
 * bluetoothd.c - Core Bluetooth daemon / 蓝牙核心守护进程
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bluetoothd.h"

int btd_init(bluetoothd_t *bt)
{
    memset(bt, 0, sizeof(*bt));
    fprintf(stderr, "[bluetoothd] initializing v%s\n", BLUETOOTHD_VERSION);

    if (adapter_get_info(bt) < 0) {
        fprintf(stderr, "[bluetoothd] WARNING: no Bluetooth adapter found\n");
    } else {
        fprintf(stderr, "[bluetoothd] adapter: %s (%s)\n",
                bt->adapter.name, bt->adapter.address);
    }

    return 0;
}

void btd_run(bluetoothd_t *bt)
{
    fprintf(stderr, "[bluetoothd] entering event loop\n");
    /* TODO: D-Bus main loop + IPC socket */
    (void)bt;
    while (1) { sleep(5); }
}

void btd_shutdown(bluetoothd_t *bt)
{
    if (bt->scanning) scan_stop(bt);
    fprintf(stderr, "[bluetoothd] shutdown\n");
}
