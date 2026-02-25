/*
 * dhcp.c - DHCP client / DHCP 客户端
 *
 * Lightweight DHCP client for obtaining IP addresses.
 * Delegates to system dhclient/udhcpc when available.
 * 轻量级 DHCP 客户端用于获取 IP 地址。
 * 可用时委托给系统 dhclient/udhcpc。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "netd.h"

int dhcp_request(const char *iface, dhcp_lease_t *lease)
{
    if (!iface || !lease) return -1;
    memset(lease, 0, sizeof(*lease));

    fprintf(stderr, "[netd] dhcp: requesting lease for %s\n", iface);

    /* Try udhcpc (BusyBox), then dhclient / 先尝试 udhcpc，再尝试 dhclient */
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "udhcpc -i %s -n -q 2>/dev/null || dhclient -1 %s 2>/dev/null",
             iface, iface);

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "[netd] dhcp: failed to obtain lease for %s\n", iface);
        return -1;
    }

    /* Read assigned IP from sysfs/ip command / 从 sysfs/ip 命令读取分配的 IP */
    char ip_cmd[256];
    snprintf(ip_cmd, sizeof(ip_cmd),
             "ip -4 addr show %s | grep -oP 'inet \\K[\\d.]+'", iface);

    FILE *fp = popen(ip_cmd, "r");
    if (fp) {
        if (fgets(lease->ip, sizeof(lease->ip), fp)) {
            lease->ip[strcspn(lease->ip, "\n")] = '\0';
        }
        pclose(fp);
    }

    lease->obtained = (uint64_t)time(NULL);
    lease->lease_time = 86400; /* Default 24h / 默认 24 小时 */

    fprintf(stderr, "[netd] dhcp: obtained %s for %s\n", lease->ip, iface);
    return 0;
}

int dhcp_release(const char *iface)
{
    fprintf(stderr, "[netd] dhcp: releasing lease for %s\n", iface);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip addr flush dev %s 2>/dev/null", iface);
    return system(cmd) == 0 ? 0 : -1;
}

int dhcp_renew(const char *iface, dhcp_lease_t *lease)
{
    fprintf(stderr, "[netd] dhcp: renewing lease for %s\n", iface);
    return dhcp_request(iface, lease);
}
