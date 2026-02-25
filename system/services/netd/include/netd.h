/*
 * netd.h - LumiOS Network Manager / 网络管理服务
 *
 * Manages Wi-Fi (via iwd/wpa_supplicant), DHCP, DNS, and network monitoring.
 * 管理 Wi-Fi（通过 iwd/wpa_supplicant）、DHCP、DNS 和网络监控。
 */

#ifndef NETD_H
#define NETD_H

#include <stdbool.h>
#include <stdint.h>

#define NETD_VERSION "0.1.0"
#define NETD_SOCKET  "/run/netd.sock"
#define NETD_CONF    "/etc/network/netd.conf"

/* === Network interface state / 网络接口状态 === */
typedef enum {
    NET_DOWN = 0,
    NET_CONNECTING,
    NET_CONNECTED,
    NET_DISCONNECTING,
    NET_FAILED,
} net_state_t;

/* === Interface type / 接口类型 === */
typedef enum {
    IFACE_WIFI = 0,
    IFACE_ETHERNET,
    IFACE_CELLULAR,
    IFACE_VPN,
    IFACE_LOOPBACK,
} iface_type_t;

/* === Network interface / 网络接口 === */
typedef struct {
    char        name[32];       /* e.g. "wlan0" */
    iface_type_t type;
    net_state_t  state;
    char        ip4[16];        /* IPv4 address / IPv4 地址 */
    char        ip6[48];        /* IPv6 address / IPv6 地址 */
    char        gateway[16];
    char        dns[2][16];     /* Primary + secondary DNS / 主+备 DNS */
    char        mac[18];
    int         signal;         /* Wi-Fi signal 0-100 / Wi-Fi 信号 */
    char        ssid[64];       /* Connected SSID / 已连接 SSID */
    uint64_t    rx_bytes;
    uint64_t    tx_bytes;
    bool        is_default;     /* Default route / 默认路由 */
} net_iface_t;

/* === Wi-Fi scan result / Wi-Fi 扫描结果 === */
typedef struct {
    char ssid[64];
    char bssid[18];
    int  signal;        /* dBm */
    int  frequency;     /* MHz */
    bool secured;
    char security[32];  /* "WPA2", "WPA3", "Open" */
} wifi_scan_t;

/* === DHCP lease / DHCP 租约 === */
typedef struct {
    char ip[16];
    char gateway[16];
    char dns1[16];
    char dns2[16];
    char subnet[16];
    uint32_t lease_time;   /* seconds / 秒 */
    uint64_t obtained;     /* timestamp / 时间戳 */
} dhcp_lease_t;

/* === Function declarations / 函数声明 === */

/* netd.c - Core daemon / 核心守护进程 */
int  netd_init(void);
void netd_run(void);
void netd_shutdown(void);

/* wifi.c - Wi-Fi management / Wi-Fi 管理 */
int  wifi_init(const char *iface);
int  wifi_scan(wifi_scan_t *results, int max);
int  wifi_connect(const char *ssid, const char *password);
int  wifi_disconnect(void);
int  wifi_get_status(net_iface_t *iface);
bool wifi_is_connected(void);

/* dhcp.c - DHCP client / DHCP 客户端 */
int  dhcp_request(const char *iface, dhcp_lease_t *lease);
int  dhcp_release(const char *iface);
int  dhcp_renew(const char *iface, dhcp_lease_t *lease);

/* dns.c - DNS resolver configuration / DNS 解析器配置 */
int  dns_set_servers(const char *dns1, const char *dns2);
int  dns_set_doh(const char *doh_url);
int  dns_flush_cache(void);

/* monitor.c - Network monitoring / 网络监控 */
int  monitor_init(void);
void monitor_update(void);
int  monitor_get_interfaces(net_iface_t *out, int max);
void monitor_get_traffic(uint64_t *rx, uint64_t *tx);

#endif /* NETD_H */
