/*
 * sms.c - SMS handling via AT commands / AT 命令短信处理
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "telephonyd.h"

int sms_send(telephonyd_t *t, const char *number, const char *body)
{
    if (!number || !body) return -1;
    if (t->airplane_mode) return -1;

    char cmd[64], resp[256];

    /* Set text mode */
    modem_send_at(t, "AT+CMGF=1", resp, sizeof(resp));

    /* Set recipient */
    snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"", number);
    if (write(t->modem_fd, cmd, strlen(cmd)) < 0) return -1;
    if (write(t->modem_fd, "\r", 1) < 0) return -1;
    usleep(500000);

    /* Send body + Ctrl-Z */
    if (write(t->modem_fd, body, strlen(body)) < 0) return -1;
    char ctrlz = 0x1A;
    if (write(t->modem_fd, &ctrlz, 1) < 0) return -1;
    usleep(3000000); /* wait up to 3s for send */

    /* Read response */
    memset(resp, 0, sizeof(resp));
    read(t->modem_fd, resp, sizeof(resp) - 1);

    if (strstr(resp, "OK")) {
        fprintf(stderr, "[telephonyd] SMS sent to %s\n", number);
        return 0;
    }

    fprintf(stderr, "[telephonyd] SMS send failed\n");
    return -1;
}

int sms_read_incoming(telephonyd_t *t, sms_pdu_t *pdu)
{
    if (!pdu) return -1;

    char resp[1024];
    /* Read unread messages */
    if (modem_send_at(t, "AT+CMGL=\"REC UNREAD\"", resp, sizeof(resp)) < 0)
        return -1;

    /* Parse first unread message */
    char *p = strstr(resp, "+CMGL:");
    if (!p) return -1;

    memset(pdu, 0, sizeof(*pdu));
    pdu->timestamp = (uint64_t)time(NULL);
    pdu->pdu_type = 0; /* deliver */

    /* Extract sender from header */
    char *quote = strchr(p, '"');
    if (quote) {
        quote++; /* skip status */
        char *end = strchr(quote, '"');
        if (end) {
            quote = strchr(end + 1, '"');
            if (quote) {
                quote++;
                end = strchr(quote, '"');
                if (end) {
                    *end = '\0';
                    strncpy(pdu->sender, quote, sizeof(pdu->sender) - 1);
                }
            }
        }
    }

    /* Body is on the next line */
    char *body = strstr(p, "\r\n");
    if (body) {
        body += 2;
        char *end = strstr(body, "\r\n");
        if (end) *end = '\0';
        strncpy(pdu->body, body, sizeof(pdu->body) - 1);
    }

    fprintf(stderr, "[telephonyd] SMS received from %s: %.40s...\n",
            pdu->sender, pdu->body);
    return 0;
}
