/*
 * sensord.c - Core sensor daemon / 传感器核心守护进程
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sensord.h"

int sensord_init(sensord_t *s)
{
    memset(s, 0, sizeof(*s));
    fprintf(stderr, "[sensord] initializing v%s\n", SENSORD_VERSION);

    s->num_sensors = sensor_enumerate(s->sensors, SENSOR_MAX_DEVICES);
    fprintf(stderr, "[sensord] found %d sensor(s)\n", s->num_sensors);

    for (int i = 0; i < s->num_sensors; i++) {
        fprintf(stderr, "[sensord]   %s: %s (%s)\n",
                sensor_type_name(s->sensors[i].type),
                s->sensors[i].name, s->sensors[i].iio_path);
    }

    fusion_init();
    s->running = true;
    return 0;
}

void sensord_run(sensord_t *s)
{
    fprintf(stderr, "[sensord] entering event loop\n");
    /* TODO: poll IIO devices + IPC socket / 轮询 IIO 设备 + IPC 套接字 */
    while (s->running) { sleep(1); }
}

void sensord_shutdown(sensord_t *s)
{
    s->running = false;
    for (int i = 0; i < s->num_sensors; i++) {
        if (s->sensors[i].active)
            sensor_disable(s, s->sensors[i].type);
    }
    fprintf(stderr, "[sensord] shutdown\n");
}
