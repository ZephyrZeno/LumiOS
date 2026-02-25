/*
 * telephonyd.c - Core telephony daemon / 电话核心守护进程
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "telephonyd.h"

int telephonyd_init(telephonyd_t *t)
{
    memset(t, 0, sizeof(*t));
    t->modem_fd = -1;
    t->active_sim = 0;
    t->airplane_mode = false;

    fprintf(stderr, "[telephonyd] initializing v%s\n", TELEPHONYD_VERSION);

    /* Try common modem device paths / 尝试常见基带设备路径 */
    const char *modem_paths[] = {
        "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2",
        "/dev/ttyACM0", "/dev/ttyACM1",
        "/dev/ttyHS0",  "/dev/ttyHS1",
        NULL
    };

    for (int i = 0; modem_paths[i]; i++) {
        if (modem_open(t, modem_paths[i]) == 0) {
            fprintf(stderr, "[telephonyd] modem found: %s\n", modem_paths[i]);
            modem_get_info(t);
            break;
        }
    }

    if (t->modem_fd < 0) {
        fprintf(stderr, "[telephonyd] WARNING: no modem found\n");
    }

    /* Get SIM info / 获取 SIM 信息 */
    for (int i = 0; i < 2; i++) {
        sim_get_state(t, i);
        if (t->sim[i].state == SIM_READY) {
            sim_get_info(t, i);
            fprintf(stderr, "[telephonyd] SIM%d: %s (%s)\n",
                    i, t->sim[i].operator_name, t->sim[i].phone_number);
        }
    }

    t->running = true;
    return 0;
}

void telephonyd_run(telephonyd_t *t)
{
    fprintf(stderr, "[telephonyd] entering event loop\n");
    /* TODO: AT command polling loop + IPC socket */
    while (t->running) { sleep(5); }
}

void telephonyd_shutdown(telephonyd_t *t)
{
    /* Hangup active calls / 挂断活动通话 */
    for (int i = 0; i < t->num_calls; i++) {
        if (t->calls[i].state == TCALL_ACTIVE ||
            t->calls[i].state == TCALL_DIALING) {
            call_hangup(t, t->calls[i].call_id);
        }
    }

    if (t->data.active) data_disconnect(t);
    modem_close(t);
    t->running = false;
    fprintf(stderr, "[telephonyd] shutdown\n");
}
