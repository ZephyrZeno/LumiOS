/*
 * data.c - Mobile data connection / 移动数据连接
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telephonyd.h"

int data_connect(telephonyd_t *t, const char *apn)
{
    if (t->airplane_mode) return -1;
    if (t->data.active) return 0;

    char cmd[128], resp[256];

    /* Define PDP context / 定义 PDP 上下文 */
    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", apn);
    if (modem_send_at(t, cmd, resp, sizeof(resp)) < 0) return -1;

    /* Activate PDP context / 激活 PDP 上下文 */
    if (modem_send_at(t, "AT+CGACT=1,1", resp, sizeof(resp)) < 0) return -1;

    /* Get assigned IP / 获取分配的 IP */
    if (modem_send_at(t, "AT+CGPADDR=1", resp, sizeof(resp)) == 0) {
        char *p = strstr(resp, "+CGPADDR:");
        if (p) {
            char *quote = strchr(p, '"');
            if (quote) {
                quote++;
                char *end = strchr(quote, '"');
                if (end) *end = '\0';
                strncpy(t->data.ip, quote, sizeof(t->data.ip) - 1);
            }
        }
    }

    strncpy(t->data.apn, apn, sizeof(t->data.apn) - 1);
    t->data.active = true;
    t->data.tech = t->signal.tech;
    t->data.rx_bytes = 0;
    t->data.tx_bytes = 0;

    fprintf(stderr, "[telephonyd] data connected: APN=%s IP=%s\n",
            apn, t->data.ip);
    return 0;
}

int data_disconnect(telephonyd_t *t)
{
    if (!t->data.active) return 0;

    char resp[256];
    modem_send_at(t, "AT+CGACT=0,1", resp, sizeof(resp));

    t->data.active = false;
    fprintf(stderr, "[telephonyd] data disconnected\n");
    return 0;
}

int data_get_status(telephonyd_t *t, data_conn_t *conn)
{
    if (!conn) return -1;
    *conn = t->data;
    return 0;
}
