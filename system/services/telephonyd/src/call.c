/*
 * call.c - Voice call control via AT commands / AT 命令通话控制
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telephonyd.h"

int call_originate(telephonyd_t *t, const char *number)
{
    if (!number || !number[0]) return -1;
    if (t->airplane_mode) return -1;

    char cmd[64], resp[256];
    snprintf(cmd, sizeof(cmd), "ATD%s;", number);

    if (modem_send_at(t, cmd, resp, sizeof(resp)) < 0) {
        fprintf(stderr, "[telephonyd] call failed: %s\n", number);
        return -1;
    }

    if (t->num_calls < 8) {
        tcall_t *c = &t->calls[t->num_calls++];
        c->state = TCALL_DIALING;
        strncpy(c->number, number, sizeof(c->number) - 1);
        c->call_id = t->num_calls;
        c->multiparty = false;
    }

    fprintf(stderr, "[telephonyd] dialing: %s\n", number);
    return t->num_calls;
}

int call_answer(telephonyd_t *t, int call_id)
{
    char resp[256];
    if (modem_send_at(t, "ATA", resp, sizeof(resp)) < 0) return -1;

    for (int i = 0; i < t->num_calls; i++) {
        if (t->calls[i].call_id == call_id) {
            t->calls[i].state = TCALL_ACTIVE;
            fprintf(stderr, "[telephonyd] call answered: %s\n", t->calls[i].number);
            return 0;
        }
    }
    return -1;
}

int call_hangup(telephonyd_t *t, int call_id)
{
    char cmd[32], resp[256];
    snprintf(cmd, sizeof(cmd), "AT+CHLD=1%d", call_id);
    modem_send_at(t, cmd, resp, sizeof(resp));

    /* Also try generic hangup */
    modem_send_at(t, "ATH", resp, sizeof(resp));

    for (int i = 0; i < t->num_calls; i++) {
        if (t->calls[i].call_id == call_id) {
            fprintf(stderr, "[telephonyd] hangup: %s\n", t->calls[i].number);
            t->calls[i].state = TCALL_RELEASED;
            /* Remove from active calls */
            for (int j = i; j < t->num_calls - 1; j++)
                t->calls[j] = t->calls[j + 1];
            t->num_calls--;
            return 0;
        }
    }
    return -1;
}

int call_hold(telephonyd_t *t, int call_id)
{
    char resp[256];
    if (modem_send_at(t, "AT+CHLD=2", resp, sizeof(resp)) < 0) return -1;

    for (int i = 0; i < t->num_calls; i++) {
        if (t->calls[i].call_id == call_id) {
            t->calls[i].state = TCALL_HELD;
            return 0;
        }
    }
    return -1;
}

int call_dtmf(telephonyd_t *t, char digit)
{
    char cmd[32], resp[256];
    snprintf(cmd, sizeof(cmd), "AT+VTS=%c", digit);
    return modem_send_at(t, cmd, resp, sizeof(resp));
}
