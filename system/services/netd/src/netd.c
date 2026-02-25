/*
 * netd.c - Core network daemon / 核心网络守护进程
 *
 * Initializes network subsystems, monitors interfaces, handles IPC.
 * 初始化网络子系统，监控接口，处理 IPC。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "netd.h"

/* === Initialize network daemon / 初始化网络守护进程 === */

int netd_init(void)
{
    fprintf(stderr, "[netd] initializing network subsystems\n");

    /* Initialize network monitor / 初始化网络监控 */
    if (monitor_init() < 0) {
        fprintf(stderr, "[netd] WARNING: monitor init failed\n");
    }

    /* Initialize Wi-Fi / 初始化 Wi-Fi */
    if (wifi_init("wlan0") < 0) {
        fprintf(stderr, "[netd] WARNING: wifi init failed (no wireless interface?)\n");
    }

    /* Configure DNS-over-HTTPS by default / 默认配置 DNS-over-HTTPS */
    dns_set_doh("https://1.1.1.1/dns-query");

    fprintf(stderr, "[netd] initialization complete\n");
    return 0;
}

/* === Main event loop / 主事件循环 === */

void netd_run(void)
{
    fprintf(stderr, "[netd] entering event loop\n");

    /* TODO: epoll-based event loop with:
     *   - IPC socket for shell/settings communication
     *   - netlink socket for interface state changes
     *   - timer for periodic monitoring
     *
     * TODO: 基于 epoll 的事件循环:
     *   - IPC 套接字用于 shell/设置通信
     *   - netlink 套接字用于接口状态变化
     *   - 定时器用于定期监控
     */

    while (1) {
        monitor_update();
        sleep(5);
    }
}

/* === Shutdown / 关闭 === */

void netd_shutdown(void)
{
    fprintf(stderr, "[netd] shutting down\n");
    wifi_disconnect();
}
