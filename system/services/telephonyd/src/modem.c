/*
 * modem.c - Modem management via AT commands / AT 命令基带管理
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include "telephonyd.h"

int modem_open(telephonyd_t *t, const char *device)
{
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;

    /* Configure serial port: 115200 8N1 */
    struct termios tio;
    if (tcgetattr(fd, &tio) < 0) {
        close(fd);
        return -1;
    }

    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cflag = CS8 | CLOCAL | CREAD;
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 10;  /* 1 second timeout */

    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIOFLUSH);

    t->modem_fd = fd;
    strncpy(t->modem.device_path, device, sizeof(t->modem.device_path) - 1);

    /* Test with AT command */
    char resp[256];
    if (modem_send_at(t, "AT", resp, sizeof(resp)) < 0) {
        close(fd);
        t->modem_fd = -1;
        return -1;
    }

    t->modem.powered = true;
    t->modem.online = true;
    return 0;
}

int modem_close(telephonyd_t *t)
{
    if (t->modem_fd >= 0) {
        close(t->modem_fd);
        t->modem_fd = -1;
    }
    t->modem.powered = false;
    t->modem.online = false;
    return 0;
}

int modem_send_at(telephonyd_t *t, const char *cmd, char *resp, size_t resp_len)
{
    if (t->modem_fd < 0) return -1;

    /* Send command + CR */
    char buf[512];
    int len = snprintf(buf, sizeof(buf), "%s\r", cmd);
    if (write(t->modem_fd, buf, len) < 0) return -1;

    /* Read response (wait up to 2s) */
    usleep(200000);
    memset(resp, 0, resp_len);
    int total = 0;

    for (int retry = 0; retry < 20 && total < (int)resp_len - 1; retry++) {
        int n = read(t->modem_fd, resp + total, resp_len - total - 1);
        if (n > 0) {
            total += n;
            if (strstr(resp, "OK") || strstr(resp, "ERROR")) break;
        }
        usleep(100000);
    }

    if (total == 0) return -1;
    return (strstr(resp, "OK") != NULL) ? 0 : -1;
}

int modem_power(telephonyd_t *t, bool on)
{
    if (on) {
        char resp[256];
        return modem_send_at(t, "AT+CFUN=1", resp, sizeof(resp));
    } else {
        char resp[256];
        return modem_send_at(t, "AT+CFUN=0", resp, sizeof(resp));
    }
}

int modem_get_info(telephonyd_t *t)
{
    char resp[256];

    if (modem_send_at(t, "AT+CGMI", resp, sizeof(resp)) == 0) {
        char *line = strchr(resp, '\n');
        if (line) {
            line++;
            char *end = strchr(line, '\r');
            if (end) *end = '\0';
            strncpy(t->modem.manufacturer, line, sizeof(t->modem.manufacturer) - 1);
        }
    }

    if (modem_send_at(t, "AT+CGMM", resp, sizeof(resp)) == 0) {
        char *line = strchr(resp, '\n');
        if (line) {
            line++;
            char *end = strchr(line, '\r');
            if (end) *end = '\0';
            strncpy(t->modem.model, line, sizeof(t->modem.model) - 1);
        }
    }

    if (modem_send_at(t, "AT+CGSN", resp, sizeof(resp)) == 0) {
        char *line = strchr(resp, '\n');
        if (line) {
            line++;
            char *end = strchr(line, '\r');
            if (end) *end = '\0';
            strncpy(t->modem.imei, line, sizeof(t->modem.imei) - 1);
        }
    }

    fprintf(stderr, "[telephonyd] modem: %s %s IMEI:%s\n",
            t->modem.manufacturer, t->modem.model, t->modem.imei);
    return 0;
}
