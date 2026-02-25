/*
 * adapter.c - Bluetooth adapter management / 蓝牙适配器管理
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bluetoothd.h"

int adapter_get_info(bluetoothd_t *bt)
{
    /* TODO: query BlueZ via D-Bus org.bluez.Adapter1 */
    /* Placeholder: check if hci0 exists */
    FILE *f = fopen("/sys/class/bluetooth/hci0/address", "r");
    if (!f) return -1;

    if (fgets(bt->adapter.address, sizeof(bt->adapter.address), f)) {
        char *nl = strchr(bt->adapter.address, '\n');
        if (nl) *nl = '\0';
    }
    fclose(f);

    f = fopen("/sys/class/bluetooth/hci0/name", "r");
    if (f) {
        if (fgets(bt->adapter.name, sizeof(bt->adapter.name), f)) {
            char *nl = strchr(bt->adapter.name, '\n');
            if (nl) *nl = '\0';
        }
        fclose(f);
    } else {
        strncpy(bt->adapter.name, "LumiOS", sizeof(bt->adapter.name) - 1);
    }

    strncpy(bt->adapter.alias, bt->adapter.name, sizeof(bt->adapter.alias) - 1);
    bt->adapter.powered = true;
    bt->adapter.pairable = true;
    bt->adapter.discoverable = false;
    bt->adapter.discoverable_timeout = 180;

    return 0;
}

int adapter_power(bluetoothd_t *bt, bool on)
{
    bt->adapter.powered = on;
    fprintf(stderr, "[bluetoothd] adapter power: %s\n", on ? "on" : "off");
    /* TODO: D-Bus call org.bluez.Adapter1.Set("Powered", on) */
    return 0;
}

int adapter_set_discoverable(bluetoothd_t *bt, bool discoverable, uint32_t timeout)
{
    bt->adapter.discoverable = discoverable;
    bt->adapter.discoverable_timeout = timeout;
    fprintf(stderr, "[bluetoothd] discoverable: %s (timeout=%us)\n",
            discoverable ? "yes" : "no", timeout);
    return 0;
}

int adapter_set_alias(bluetoothd_t *bt, const char *alias)
{
    strncpy(bt->adapter.alias, alias, sizeof(bt->adapter.alias) - 1);
    fprintf(stderr, "[bluetoothd] alias: %s\n", alias);
    return 0;
}
