/*
 * monitor.c - Network interface monitoring / 网络接口监控
 *
 * Reads interface state from sysfs and tracks traffic stats.
 * 从 sysfs 读取接口状态并跟踪流量统计。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "netd.h"

static net_iface_t g_ifaces[16];
static int g_iface_count = 0;

/* === Read sysfs value / 读取 sysfs 值 === */

static int read_sysfs(const char *path, char *buf, int size)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    if (!fgets(buf, size, fp)) { fclose(fp); return -1; }
    buf[strcspn(buf, "\n")] = '\0';
    fclose(fp);
    return 0;
}

static uint64_t read_sysfs_u64(const char *path)
{
    char buf[32];
    if (read_sysfs(path, buf, sizeof(buf)) < 0) return 0;
    return (uint64_t)strtoull(buf, NULL, 10);
}

/* === Initialize monitor / 初始化监控 === */

int monitor_init(void)
{
    g_iface_count = 0;
    fprintf(stderr, "[netd] monitor: initialized\n");
    return 0;
}

/* === Update interface list / 更新接口列表 === */

void monitor_update(void)
{
    DIR *d = opendir("/sys/class/net");
    if (!d) return;

    g_iface_count = 0;
    struct dirent *ent;

    while ((ent = readdir(d)) != NULL && g_iface_count < 16) {
        if (ent->d_name[0] == '.') continue;

        net_iface_t *iface = &g_ifaces[g_iface_count];
        memset(iface, 0, sizeof(*iface));
        strncpy(iface->name, ent->d_name, sizeof(iface->name) - 1);

        /* Detect type / 检测类型 */
        char path[256];
        snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", ent->d_name);
        if (access(path, F_OK) == 0) {
            iface->type = IFACE_WIFI;
        } else if (strcmp(ent->d_name, "lo") == 0) {
            iface->type = IFACE_LOOPBACK;
        } else {
            iface->type = IFACE_ETHERNET;
        }

        /* Read state / 读取状态 */
        char state[32];
        snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", ent->d_name);
        if (read_sysfs(path, state, sizeof(state)) == 0) {
            if (strcmp(state, "up") == 0)
                iface->state = NET_CONNECTED;
            else if (strcmp(state, "down") == 0)
                iface->state = NET_DOWN;
        }

        /* Read MAC address / 读取 MAC 地址 */
        snprintf(path, sizeof(path), "/sys/class/net/%s/address", ent->d_name);
        read_sysfs(path, iface->mac, sizeof(iface->mac));

        /* Read traffic stats / 读取流量统计 */
        snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", ent->d_name);
        iface->rx_bytes = read_sysfs_u64(path);
        snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", ent->d_name);
        iface->tx_bytes = read_sysfs_u64(path);

        g_iface_count++;
    }

    closedir(d);
}

/* === Get interface list / 获取接口列表 === */

int monitor_get_interfaces(net_iface_t *out, int max)
{
    int count = (g_iface_count < max) ? g_iface_count : max;
    memcpy(out, g_ifaces, sizeof(net_iface_t) * (size_t)count);
    return count;
}

/* === Get total traffic / 获取总流量 === */

void monitor_get_traffic(uint64_t *rx, uint64_t *tx)
{
    *rx = 0; *tx = 0;
    for (int i = 0; i < g_iface_count; i++) {
        if (g_ifaces[i].type == IFACE_LOOPBACK) continue;
        *rx += g_ifaces[i].rx_bytes;
        *tx += g_ifaces[i].tx_bytes;
    }
}
