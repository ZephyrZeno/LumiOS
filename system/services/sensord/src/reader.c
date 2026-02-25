/*
 * reader.c - Sensor data reading via IIO sysfs / 通过 IIO sysfs 读取传感器数据
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sensord.h"

static sensor_info_t *find_sensor(sensord_t *s, sensor_type_t type)
{
    for (int i = 0; i < s->num_sensors; i++) {
        if (s->sensors[i].type == type) return &s->sensors[i];
    }
    return NULL;
}

static float read_iio_float(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0.0f;
    float val = 0.0f;
    if (fscanf(f, "%f", &val) != 1) val = 0.0f;
    fclose(f);
    return val;
}

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int sensor_enable(sensord_t *s, sensor_type_t type, int rate_hz)
{
    sensor_info_t *si = find_sensor(s, type);
    if (!si) return -1;

    /* Set sampling frequency if supported */
    char path[256];
    snprintf(path, sizeof(path), "%s/sampling_frequency", si->iio_path);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d", rate_hz);
        fclose(f);
    }

    /* Enable buffer if available */
    snprintf(path, sizeof(path), "%s/buffer/enable", si->iio_path);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "1");
        fclose(f);
    }

    si->active = true;
    si->rate_hz = rate_hz;
    fprintf(stderr, "[sensord] enabled %s @ %dHz\n", sensor_type_name(type), rate_hz);
    return 0;
}

int sensor_disable(sensord_t *s, sensor_type_t type)
{
    sensor_info_t *si = find_sensor(s, type);
    if (!si) return -1;

    char path[256];
    snprintf(path, sizeof(path), "%s/buffer/enable", si->iio_path);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "0");
        fclose(f);
    }

    si->active = false;
    si->rate_hz = 0;
    fprintf(stderr, "[sensord] disabled %s\n", sensor_type_name(type));
    return 0;
}

int sensor_read(sensord_t *s, sensor_type_t type, sensor_event_t *event)
{
    sensor_info_t *si = find_sensor(s, type);
    if (!si) return -1;

    memset(event, 0, sizeof(*event));
    event->type = type;
    event->timestamp = now_ns();

    char path[256];
    float scale = 1.0f;

    switch (type) {
    case SENSOR_ACCEL:
        snprintf(path, sizeof(path), "%s/in_accel_scale", si->iio_path);
        scale = read_iio_float(path);
        if (scale == 0.0f) scale = 1.0f;
        snprintf(path, sizeof(path), "%s/in_accel_x_raw", si->iio_path);
        event->data.accel.x = read_iio_float(path) * scale;
        snprintf(path, sizeof(path), "%s/in_accel_y_raw", si->iio_path);
        event->data.accel.y = read_iio_float(path) * scale;
        snprintf(path, sizeof(path), "%s/in_accel_z_raw", si->iio_path);
        event->data.accel.z = read_iio_float(path) * scale;
        break;

    case SENSOR_GYRO:
        snprintf(path, sizeof(path), "%s/in_anglvel_scale", si->iio_path);
        scale = read_iio_float(path);
        if (scale == 0.0f) scale = 1.0f;
        snprintf(path, sizeof(path), "%s/in_anglvel_x_raw", si->iio_path);
        event->data.gyro.x = read_iio_float(path) * scale;
        snprintf(path, sizeof(path), "%s/in_anglvel_y_raw", si->iio_path);
        event->data.gyro.y = read_iio_float(path) * scale;
        snprintf(path, sizeof(path), "%s/in_anglvel_z_raw", si->iio_path);
        event->data.gyro.z = read_iio_float(path) * scale;
        break;

    case SENSOR_MAGNET:
        snprintf(path, sizeof(path), "%s/in_magn_scale", si->iio_path);
        scale = read_iio_float(path);
        if (scale == 0.0f) scale = 1.0f;
        snprintf(path, sizeof(path), "%s/in_magn_x_raw", si->iio_path);
        event->data.magnet.x = read_iio_float(path) * scale;
        snprintf(path, sizeof(path), "%s/in_magn_y_raw", si->iio_path);
        event->data.magnet.y = read_iio_float(path) * scale;
        snprintf(path, sizeof(path), "%s/in_magn_z_raw", si->iio_path);
        event->data.magnet.z = read_iio_float(path) * scale;
        break;

    case SENSOR_LIGHT:
        snprintf(path, sizeof(path), "%s/in_illuminance_raw", si->iio_path);
        event->data.light = read_iio_float(path);
        snprintf(path, sizeof(path), "%s/in_illuminance_scale", si->iio_path);
        scale = read_iio_float(path);
        if (scale > 0.0f) event->data.light *= scale;
        break;

    case SENSOR_PROXIMITY:
        snprintf(path, sizeof(path), "%s/in_proximity_raw", si->iio_path);
        event->data.proximity = read_iio_float(path);
        break;

    case SENSOR_PRESSURE:
        snprintf(path, sizeof(path), "%s/in_pressure_raw", si->iio_path);
        event->data.pressure = read_iio_float(path);
        snprintf(path, sizeof(path), "%s/in_pressure_scale", si->iio_path);
        scale = read_iio_float(path);
        if (scale > 0.0f) event->data.pressure *= scale;
        break;

    case SENSOR_TEMPERATURE:
        snprintf(path, sizeof(path), "%s/in_temp_raw", si->iio_path);
        event->data.temperature = read_iio_float(path);
        snprintf(path, sizeof(path), "%s/in_temp_scale", si->iio_path);
        scale = read_iio_float(path);
        if (scale > 0.0f) event->data.temperature *= scale;
        break;

    case SENSOR_HUMIDITY:
        snprintf(path, sizeof(path), "%s/in_humidityrelative_raw", si->iio_path);
        event->data.humidity = read_iio_float(path);
        break;

    case SENSOR_STEP_COUNTER:
        snprintf(path, sizeof(path), "%s/in_steps_input", si->iio_path);
        event->data.steps = (uint32_t)read_iio_float(path);
        break;

    default:
        return -1;
    }

    /* Cache last event */
    for (int i = 0; i < s->num_sensors; i++) {
        if (s->sensors[i].type == type) {
            s->last_events[i] = *event;
            break;
        }
    }

    return 0;
}

int sensor_poll(sensord_t *s, sensor_event_t *events, int max, int timeout_ms)
{
    (void)timeout_ms;
    int count = 0;
    for (int i = 0; i < s->num_sensors && count < max; i++) {
        if (s->sensors[i].active) {
            sensor_read(s, s->sensors[i].type, &events[count]);
            count++;
        }
    }
    return count;
}
