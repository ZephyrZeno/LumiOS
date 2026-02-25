/*
 * enumerate.c - Sensor enumeration via IIO / 通过 IIO 枚举传感器
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "sensord.h"

const char *sensor_type_name(sensor_type_t type)
{
    switch (type) {
    case SENSOR_ACCEL:       return "accelerometer";
    case SENSOR_GYRO:        return "gyroscope";
    case SENSOR_MAGNET:      return "magnetometer";
    case SENSOR_LIGHT:       return "light";
    case SENSOR_PROXIMITY:   return "proximity";
    case SENSOR_PRESSURE:    return "pressure";
    case SENSOR_TEMPERATURE: return "temperature";
    case SENSOR_HUMIDITY:    return "humidity";
    case SENSOR_GRAVITY:     return "gravity";
    case SENSOR_ROTATION:    return "rotation";
    case SENSOR_STEP_COUNTER:return "step_counter";
    default:                 return "unknown";
    }
}

static sensor_type_t detect_type(const char *iio_path)
{
    char path[256], buf[64];

    snprintf(path, sizeof(path), "%s/in_accel_x_raw", iio_path);
    FILE *f = fopen(path, "r");
    if (f) { fclose(f); return SENSOR_ACCEL; }

    snprintf(path, sizeof(path), "%s/in_anglvel_x_raw", iio_path);
    f = fopen(path, "r");
    if (f) { fclose(f); return SENSOR_GYRO; }

    snprintf(path, sizeof(path), "%s/in_magn_x_raw", iio_path);
    f = fopen(path, "r");
    if (f) { fclose(f); return SENSOR_MAGNET; }

    snprintf(path, sizeof(path), "%s/in_illuminance_raw", iio_path);
    f = fopen(path, "r");
    if (f) { fclose(f); return SENSOR_LIGHT; }

    snprintf(path, sizeof(path), "%s/in_proximity_raw", iio_path);
    f = fopen(path, "r");
    if (f) { fclose(f); return SENSOR_PROXIMITY; }

    snprintf(path, sizeof(path), "%s/in_pressure_raw", iio_path);
    f = fopen(path, "r");
    if (f) { fclose(f); return SENSOR_PRESSURE; }

    snprintf(path, sizeof(path), "%s/in_temp_raw", iio_path);
    f = fopen(path, "r");
    if (f) { fclose(f); return SENSOR_TEMPERATURE; }

    snprintf(path, sizeof(path), "%s/in_humidityrelative_raw", iio_path);
    f = fopen(path, "r");
    if (f) { fclose(f); return SENSOR_HUMIDITY; }

    snprintf(path, sizeof(path), "%s/in_steps_input", iio_path);
    f = fopen(path, "r");
    if (f) { fclose(f); return SENSOR_STEP_COUNTER; }

    /* Read name for heuristic */
    snprintf(path, sizeof(path), "%s/name", iio_path);
    f = fopen(path, "r");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            if (strstr(buf, "accel")) { fclose(f); return SENSOR_ACCEL; }
            if (strstr(buf, "gyro"))  { fclose(f); return SENSOR_GYRO; }
            if (strstr(buf, "magn"))  { fclose(f); return SENSOR_MAGNET; }
            if (strstr(buf, "light")) { fclose(f); return SENSOR_LIGHT; }
            if (strstr(buf, "prox"))  { fclose(f); return SENSOR_PROXIMITY; }
        }
        fclose(f);
    }

    return SENSOR_ACCEL; /* fallback */
}

int sensor_enumerate(sensor_info_t *out, int max)
{
    DIR *d = opendir("/sys/bus/iio/devices");
    if (!d) return 0;

    int count = 0;
    struct dirent *ent;

    while ((ent = readdir(d)) && count < max) {
        if (strncmp(ent->d_name, "iio:device", 10) != 0) continue;

        sensor_info_t *s = &out[count];
        memset(s, 0, sizeof(*s));
        snprintf(s->iio_path, sizeof(s->iio_path),
                 "/sys/bus/iio/devices/%s", ent->d_name);

        /* Read device name */
        char path[256];
        snprintf(path, sizeof(path), "%s/name", s->iio_path);
        FILE *f = fopen(path, "r");
        if (f) {
            if (fgets(s->name, sizeof(s->name), f)) {
                char *nl = strchr(s->name, '\n');
                if (nl) *nl = '\0';
            }
            fclose(f);
        } else {
            strncpy(s->name, ent->d_name, sizeof(s->name) - 1);
        }

        s->type = detect_type(s->iio_path);
        s->active = false;
        s->rate_hz = 0;
        count++;
    }

    closedir(d);
    return count;
}
