/*
 * sim.c - SIM card management / SIM 卡管理
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telephonyd.h"

int sim_get_state(telephonyd_t *t, int slot)
{
    if (slot < 0 || slot > 1) return -1;

    char resp[256];
    if (modem_send_at(t, "AT+CPIN?", resp, sizeof(resp)) < 0) {
        t->sim[slot].state = SIM_ABSENT;
        return -1;
    }

    if (strstr(resp, "READY")) {
        t->sim[slot].state = SIM_READY;
    } else if (strstr(resp, "SIM PIN")) {
        t->sim[slot].state = SIM_PIN_REQUIRED;
    } else if (strstr(resp, "SIM PUK")) {
        t->sim[slot].state = SIM_PUK_REQUIRED;
    } else {
        t->sim[slot].state = SIM_NOT_READY;
    }

    t->sim[slot].slot = slot;
    return 0;
}

int sim_enter_pin(telephonyd_t *t, int slot, const char *pin)
{
    if (slot < 0 || slot > 1) return -1;
    if (t->sim[slot].state != SIM_PIN_REQUIRED) return -1;

    char cmd[64], resp[256];
    snprintf(cmd, sizeof(cmd), "AT+CPIN=\"%s\"", pin);

    if (modem_send_at(t, cmd, resp, sizeof(resp)) < 0) {
        fprintf(stderr, "[telephonyd] PIN rejected\n");
        return -1;
    }

    t->sim[slot].state = SIM_READY;
    fprintf(stderr, "[telephonyd] SIM%d PIN accepted\n", slot);
    return 0;
}

int sim_get_info(telephonyd_t *t, int slot)
{
    if (slot < 0 || slot > 1) return -1;
    if (t->sim[slot].state != SIM_READY) return -1;

    char resp[256];

    /* Get IMSI */
    if (modem_send_at(t, "AT+CIMI", resp, sizeof(resp)) == 0) {
        char *line = strchr(resp, '\n');
        if (line) {
            line++;
            char *end = strchr(line, '\r');
            if (end) *end = '\0';
            strncpy(t->sim[slot].imsi, line, sizeof(t->sim[slot].imsi) - 1);
            /* Extract MCC/MNC from IMSI */
            if (strlen(t->sim[slot].imsi) >= 5) {
                strncpy(t->sim[slot].operator_mcc, t->sim[slot].imsi, 3);
                strncpy(t->sim[slot].operator_mnc, t->sim[slot].imsi + 3, 2);
            }
        }
    }

    /* Get ICCID */
    if (modem_send_at(t, "AT+CCID", resp, sizeof(resp)) == 0) {
        char *line = strchr(resp, ':');
        if (line) {
            line++;
            while (*line == ' ') line++;
            char *end = strchr(line, '\r');
            if (end) *end = '\0';
            strncpy(t->sim[slot].iccid, line, sizeof(t->sim[slot].iccid) - 1);
        }
    }

    /* Get operator name */
    if (modem_send_at(t, "AT+COPS?", resp, sizeof(resp)) == 0) {
        char *quote = strchr(resp, '"');
        if (quote) {
            quote++;
            char *end = strchr(quote, '"');
            if (end) *end = '\0';
            strncpy(t->sim[slot].operator_name, quote,
                    sizeof(t->sim[slot].operator_name) - 1);
        }
    }

    /* Get phone number */
    if (modem_send_at(t, "AT+CNUM", resp, sizeof(resp)) == 0) {
        char *quote = strchr(resp, '"');
        if (quote) {
            quote++;
            char *end = strchr(quote, '"');
            if (end) {
                *end = '\0';
                char *num = strchr(end + 1, '"');
                if (num) {
                    num++;
                    end = strchr(num, '"');
                    if (end) *end = '\0';
                    strncpy(t->sim[slot].phone_number, num,
                            sizeof(t->sim[slot].phone_number) - 1);
                }
            }
        }
    }

    return 0;
}

int sim_switch(telephonyd_t *t, int slot)
{
    if (slot < 0 || slot > 1) return -1;
    if (t->sim[slot].state != SIM_READY) return -1;

    t->active_sim = slot;
    fprintf(stderr, "[telephonyd] switched to SIM%d\n", slot);
    /* TODO: AT command to switch active SIM on dual-SIM modems */
    return 0;
}
