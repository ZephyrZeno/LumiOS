/*
 * pairing.c - Bluetooth device pairing / 蓝牙设备配对
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bluetoothd.h"

static bt_device_t *find_device(bluetoothd_t *bt, const char *address)
{
    for (int i = 0; i < bt->num_devices; i++) {
        if (strcmp(bt->devices[i].address, address) == 0)
            return &bt->devices[i];
    }
    return NULL;
}

int pair_device(bluetoothd_t *bt, const char *address)
{
    bt_device_t *dev = find_device(bt, address);
    if (!dev) {
        fprintf(stderr, "[bluetoothd] device %s not found\n", address);
        return -1;
    }

    dev->state = BT_STATE_PAIRED;
    dev->paired = true;
    fprintf(stderr, "[bluetoothd] paired: %s (%s)\n", dev->name, address);
    /* TODO: D-Bus call org.bluez.Device1.Pair() */
    return 0;
}

int unpair_device(bluetoothd_t *bt, const char *address)
{
    bt_device_t *dev = find_device(bt, address);
    if (!dev) return -1;

    dev->state = BT_STATE_DISCONNECTED;
    dev->paired = false;
    dev->trusted = false;
    fprintf(stderr, "[bluetoothd] unpaired: %s\n", address);
    /* TODO: D-Bus call org.bluez.Adapter1.RemoveDevice() */
    return 0;
}

int connect_device(bluetoothd_t *bt, const char *address)
{
    bt_device_t *dev = find_device(bt, address);
    if (!dev) return -1;

    if (!dev->paired) {
        fprintf(stderr, "[bluetoothd] device not paired, pairing first\n");
        if (pair_device(bt, address) < 0) return -1;
    }

    dev->state = BT_STATE_CONNECTING;
    fprintf(stderr, "[bluetoothd] connecting: %s (%s)\n", dev->name, address);
    /* TODO: D-Bus call org.bluez.Device1.Connect() */

    dev->state = BT_STATE_CONNECTED;
    fprintf(stderr, "[bluetoothd] connected: %s\n", dev->name);
    return 0;
}

int disconnect_device(bluetoothd_t *bt, const char *address)
{
    bt_device_t *dev = find_device(bt, address);
    if (!dev) return -1;

    dev->state = BT_STATE_PAIRED;
    fprintf(stderr, "[bluetoothd] disconnected: %s\n", dev->name);
    /* TODO: D-Bus call org.bluez.Device1.Disconnect() */
    return 0;
}

int trust_device(bluetoothd_t *bt, const char *address, bool trust)
{
    bt_device_t *dev = find_device(bt, address);
    if (!dev) return -1;
    dev->trusted = trust;
    fprintf(stderr, "[bluetoothd] trust %s: %s\n", address, trust ? "yes" : "no");
    return 0;
}

int block_device(bluetoothd_t *bt, const char *address, bool block)
{
    bt_device_t *dev = find_device(bt, address);
    if (!dev) return -1;
    dev->blocked = block;
    fprintf(stderr, "[bluetoothd] block %s: %s\n", address, block ? "yes" : "no");
    return 0;
}
