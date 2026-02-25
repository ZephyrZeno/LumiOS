/*
 * scan.c - Bluetooth device discovery / 蓝牙设备发现
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bluetoothd.h"

int scan_start(bluetoothd_t *bt)
{
    if (bt->scanning) return 0;
    if (!bt->adapter.powered) {
        fprintf(stderr, "[bluetoothd] adapter not powered\n");
        return -1;
    }

    bt->scanning = true;
    bt->num_devices = 0;
    fprintf(stderr, "[bluetoothd] scan started\n");
    /* TODO: D-Bus call org.bluez.Adapter1.StartDiscovery() */
    return 0;
}

int scan_stop(bluetoothd_t *bt)
{
    if (!bt->scanning) return 0;
    bt->scanning = false;
    fprintf(stderr, "[bluetoothd] scan stopped, found %d devices\n", bt->num_devices);
    /* TODO: D-Bus call org.bluez.Adapter1.StopDiscovery() */
    return 0;
}

int scan_get_results(const bluetoothd_t *bt, bt_device_t *out, int max)
{
    int count = (bt->num_devices < max) ? bt->num_devices : max;
    for (int i = 0; i < count; i++) {
        out[i] = bt->devices[i];
    }
    return count;
}
