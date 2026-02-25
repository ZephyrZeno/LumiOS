/*
 * network.c - Cellular network registration / 蜂窝网络注册
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telephonyd.h"

int network_register(telephonyd_t *t)
{
    char resp[256];
    if (modem_send_at(t, "AT+CREG=1", resp, sizeof(resp)) < 0) return -1;
    if (modem_send_at(t, "AT+COPS=0", resp, sizeof(resp)) < 0) return -1;

    t->reg_state = NET_REG_SEARCHING;
    fprintf(stderr, "[telephonyd] network registration initiated\n");
    return 0;
}

int network_get_signal(telephonyd_t *t, signal_info_t *sig)
{
    char resp[256];
    if (modem_send_at(t, "AT+CSQ", resp, sizeof(resp)) < 0) return -1;

    int csq = 0;
    char *p = strstr(resp, "+CSQ:");
    if (p) sscanf(p, "+CSQ: %d", &csq);

    sig->rssi = (csq == 99) ? -120 : (-113 + csq * 2);
    sig->level = (csq >= 20) ? 5 : (csq >= 15) ? 4 : (csq >= 10) ? 3 :
                 (csq >= 5) ? 2 : (csq >= 1) ? 1 : 0;
    sig->tech = RAT_LTE; /* TODO: detect from AT+COPS? */

    t->signal = *sig;
    return 0;
}

int network_get_operator(telephonyd_t *t, char *name, size_t len)
{
    char resp[256];
    if (modem_send_at(t, "AT+COPS?", resp, sizeof(resp)) < 0) return -1;

    char *quote = strchr(resp, '"');
    if (quote) {
        quote++;
        char *end = strchr(quote, '"');
        if (end) *end = '\0';
        strncpy(name, quote, len - 1);
    }
    return 0;
}

int network_set_preferred_tech(telephonyd_t *t, radio_tech_t tech)
{
    char cmd[64], resp[256];
    int mode;
    switch (tech) {
    case RAT_GSM:  mode = 13; break;
    case RAT_UMTS: mode = 14; break;
    case RAT_LTE:  mode = 38; break;
    case RAT_NR:   mode = 71; break;
    default:       mode = 2;  break; /* auto */
    }
    snprintf(cmd, sizeof(cmd), "AT+CNMP=%d", mode);
    return modem_send_at(t, cmd, resp, sizeof(resp));
}

int network_set_airplane(telephonyd_t *t, bool enabled)
{
    char resp[256];
    t->airplane_mode = enabled;
    if (enabled) {
        modem_send_at(t, "AT+CFUN=4", resp, sizeof(resp)); /* radio off */
        t->reg_state = NET_REG_NOT_REGISTERED;
    } else {
        modem_send_at(t, "AT+CFUN=1", resp, sizeof(resp)); /* radio on */
        network_register(t);
    }
    fprintf(stderr, "[telephonyd] airplane mode: %s\n", enabled ? "on" : "off");
    return 0;
}
