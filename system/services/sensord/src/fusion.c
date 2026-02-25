/*
 * fusion.c - Sensor fusion / 传感器融合
 *
 * Complementary filter combining accelerometer, gyroscope, and magnetometer
 * to compute orientation, gravity vector, and rotation.
 * 互补滤波器融合加速度计、陀螺仪和磁力计，计算姿态、重力和旋转。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "sensord.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static struct {
    float pitch;    /* radians */
    float roll;     /* radians */
    float yaw;      /* radians */
    sensor_vec3_t gravity;
    sensor_vec3_t rotation;
    bool  initialized;
    float alpha;    /* complementary filter coefficient */
} fusion;

int fusion_init(void)
{
    memset(&fusion, 0, sizeof(fusion));
    fusion.alpha = 0.98f;  /* 98% gyro, 2% accel */
    fusion.gravity.z = 9.81f;
    fprintf(stderr, "[sensord] fusion initialized (alpha=%.2f)\n", fusion.alpha);
    return 0;
}

void fusion_update(const sensor_event_t *accel, const sensor_event_t *gyro,
                   const sensor_event_t *magnet)
{
    if (!accel) return;

    float ax = accel->data.accel.x;
    float ay = accel->data.accel.y;
    float az = accel->data.accel.z;

    /* Accel-based pitch/roll / 加速度计计算俯仰/横滚 */
    float accel_pitch = atan2f(-ax, sqrtf(ay * ay + az * az));
    float accel_roll  = atan2f(ay, az);

    if (!fusion.initialized) {
        fusion.pitch = accel_pitch;
        fusion.roll  = accel_roll;
        fusion.yaw   = 0.0f;
        fusion.initialized = true;
    }

    if (gyro) {
        /* Gyro integration + complementary filter / 陀螺仪积分+互补滤波 */
        float dt = 0.01f;  /* assume 100Hz, TODO: compute from timestamps */
        float gx = gyro->data.gyro.x;
        float gy = gyro->data.gyro.y;
        float gz = gyro->data.gyro.z;

        float gyro_pitch = fusion.pitch + gx * dt;
        float gyro_roll  = fusion.roll  + gy * dt;
        float gyro_yaw   = fusion.yaw   + gz * dt;

        fusion.pitch = fusion.alpha * gyro_pitch + (1.0f - fusion.alpha) * accel_pitch;
        fusion.roll  = fusion.alpha * gyro_roll  + (1.0f - fusion.alpha) * accel_roll;
        fusion.yaw   = gyro_yaw;
    } else {
        fusion.pitch = accel_pitch;
        fusion.roll  = accel_roll;
    }

    if (magnet) {
        /* Magnetometer-based yaw correction / 磁力计校正航向 */
        float mx = magnet->data.magnet.x;
        float my = magnet->data.magnet.y;
        float mag_yaw = atan2f(-my, mx);
        fusion.yaw = fusion.alpha * fusion.yaw + (1.0f - fusion.alpha) * mag_yaw;
    }

    /* Update gravity vector / 更新重力向量 */
    fusion.gravity.x = -9.81f * sinf(fusion.pitch);
    fusion.gravity.y =  9.81f * sinf(fusion.roll) * cosf(fusion.pitch);
    fusion.gravity.z =  9.81f * cosf(fusion.roll) * cosf(fusion.pitch);

    /* Update rotation vector (simplified Euler→quaternion xyz) */
    fusion.rotation.x = fusion.pitch;
    fusion.rotation.y = fusion.roll;
    fusion.rotation.z = fusion.yaw;
}

int fusion_get_orientation(float *pitch, float *roll, float *yaw)
{
    if (!fusion.initialized) return -1;
    if (pitch) *pitch = fusion.pitch * 180.0f / M_PI;
    if (roll)  *roll  = fusion.roll  * 180.0f / M_PI;
    if (yaw)   *yaw   = fusion.yaw   * 180.0f / M_PI;
    return 0;
}

int fusion_get_gravity(sensor_vec3_t *gravity)
{
    if (!fusion.initialized) return -1;
    *gravity = fusion.gravity;
    return 0;
}

int fusion_get_rotation(sensor_vec3_t *rotation)
{
    if (!fusion.initialized) return -1;
    *rotation = fusion.rotation;
    return 0;
}
