/*
 * bluetoothd.h - LumiOS Bluetooth Service / 蓝牙服务
 *
 * Manages Bluetooth devices via BlueZ D-Bus API.
 * 通过 BlueZ D-Bus API 管理蓝牙设备。
 */

#ifndef BLUETOOTHD_H
#define BLUETOOTHD_H

#include <stdbool.h>
#include <stdint.h>

#define BLUETOOTHD_VERSION "0.1.0"
#define BLUETOOTHD_SOCKET  "/run/bluetoothd.sock"
#define BT_MAX_DEVICES     64

/* === Bluetooth device type / 蓝牙设备类型 === */
typedef enum {
    BT_TYPE_UNKNOWN = 0,
    BT_TYPE_PHONE,
    BT_TYPE_COMPUTER,
    BT_TYPE_HEADSET,
    BT_TYPE_SPEAKER,
    BT_TYPE_KEYBOARD,
    BT_TYPE_MOUSE,
    BT_TYPE_GAMEPAD,
    BT_TYPE_WATCH,
    BT_TYPE_CAR,
} bt_type_t;

/* === Connection state / 连接状态 === */
typedef enum {
    BT_STATE_DISCONNECTED = 0,
    BT_STATE_CONNECTING,
    BT_STATE_CONNECTED,
    BT_STATE_PAIRED,
    BT_STATE_BONDED,
} bt_state_t;

/* === Bluetooth device / 蓝牙设备 === */
typedef struct {
    char        address[18];      /* XX:XX:XX:XX:XX:XX */
    char        name[128];
    bt_type_t   type;
    bt_state_t  state;
    int16_t     rssi;             /* signal strength dBm */
    bool        paired;
    bool        trusted;
    bool        blocked;
    uint32_t    class_of_device;
    char        icon[32];         /* BlueZ icon name */
    uint8_t     battery;          /* 0-100, 0 = unknown */
} bt_device_t;

/* === Adapter info / 适配器信息 === */
typedef struct {
    char    address[18];
    char    name[128];
    char    alias[128];
    bool    powered;
    bool    discoverable;
    bool    pairable;
    uint32_t discoverable_timeout;
} bt_adapter_t;

/* === Bluetoothd instance / 蓝牙服务实例 === */
typedef struct {
    bt_adapter_t adapter;
    bt_device_t  devices[BT_MAX_DEVICES];
    int          num_devices;
    bool         scanning;
} bluetoothd_t;

/* === Function declarations / 函数声明 === */

/* bluetoothd.c - Core daemon */
int  btd_init(bluetoothd_t *bt);
void btd_run(bluetoothd_t *bt);
void btd_shutdown(bluetoothd_t *bt);

/* adapter.c - Adapter management / 适配器管理 */
int  adapter_power(bluetoothd_t *bt, bool on);
int  adapter_set_discoverable(bluetoothd_t *bt, bool discoverable, uint32_t timeout);
int  adapter_set_alias(bluetoothd_t *bt, const char *alias);
int  adapter_get_info(bluetoothd_t *bt);

/* scan.c - Device discovery / 设备发现 */
int  scan_start(bluetoothd_t *bt);
int  scan_stop(bluetoothd_t *bt);
int  scan_get_results(const bluetoothd_t *bt, bt_device_t *out, int max);

/* pairing.c - Device pairing / 设备配对 */
int  pair_device(bluetoothd_t *bt, const char *address);
int  unpair_device(bluetoothd_t *bt, const char *address);
int  connect_device(bluetoothd_t *bt, const char *address);
int  disconnect_device(bluetoothd_t *bt, const char *address);
int  trust_device(bluetoothd_t *bt, const char *address, bool trust);
int  block_device(bluetoothd_t *bt, const char *address, bool block);

#endif /* BLUETOOTHD_H */
