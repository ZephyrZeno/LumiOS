/*
 * wifi.c - Wi-Fi management / Wi-Fi 管理
 *
 * Interfaces with iwd/wpa_supplicant for wireless connectivity.
 * 通过 iwd/wpa_supplicant 接口管理无线连接。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include "netd.h"

static char g_wifi_iface[32] = "wlan0";
static bool g_wifi_initialized = false;

int wifi_init(const char *iface)
{
    if (iface) strncpy(g_wifi_iface, iface, sizeof(g_wifi_iface) - 1);

    /* Check if wireless interface exists / 检查无线接口是否存在 */
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", g_wifi_iface);

    if (access(path, F_OK) < 0) {
        fprintf(stderr, "[netd] wifi: interface %s not found\n", g_wifi_iface);
        return -1;
    }

    /* Bring interface up / 启动接口 */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip link set %s up 2>/dev/null", g_wifi_iface);
    system(cmd);

    g_wifi_initialized = true;
    fprintf(stderr, "[netd] wifi: initialized on %s\n", g_wifi_iface);
    return 0;
}

int wifi_scan(wifi_scan_t *results, int max)
{
    if (!g_wifi_initialized) return -1;
    (void)results; (void)max;

    /* TODO: trigger scan via iwd D-Bus or wpa_cli / TODO: 通过 iwd D-Bus 或 wpa_cli 触发扫描 */
    fprintf(stderr, "[netd] wifi: scan requested\n");
    return 0;
}

int wifi_connect(const char *ssid, const char *password)
{
    if (!g_wifi_initialized) return -1;

    fprintf(stderr, "[netd] wifi: connecting to '%s'\n", ssid);

    /* TODO: connect via iwd or wpa_supplicant / TODO: 通过 iwd 或 wpa_supplicant 连接 */
    (void)password;
    return 0;
}

int wifi_disconnect(void)
{
    if (!g_wifi_initialized) return 0;
    fprintf(stderr, "[netd] wifi: disconnecting\n");
    return 0;
}

int wifi_get_status(net_iface_t *iface)
{
    if (!g_wifi_initialized) return -1;
    memset(iface, 0, sizeof(*iface));
    strncpy(iface->name, g_wifi_iface, sizeof(iface->name) - 1);
    iface->type = IFACE_WIFI;
    iface->state = NET_DOWN;

    /* Read from sysfs / 从 sysfs 读取 */
    char path[256], buf[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", g_wifi_iface);
    FILE *fp = fopen(path, "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            if (strncmp(buf, "up", 2) == 0) iface->state = NET_CONNECTED;
        }
        fclose(fp);
    }

    return 0;
}

bool wifi_is_connected(void)
{
    net_iface_t iface;
    wifi_get_status(&iface);
    return iface.state == NET_CONNECTED;
}
