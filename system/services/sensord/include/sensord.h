/*
 * sensord.h - LumiOS Sensor Service / 传感器服务
 *
 * Reads hardware sensors via /sys/bus/iio and provides unified API.
 * 通过 /sys/bus/iio 读取硬件传感器并提供统一 API。
 */

#ifndef SENSORD_H
#define SENSORD_H

#include <stdbool.h>
#include <stdint.h>

#define SENSORD_VERSION "0.1.0"
#define SENSORD_SOCKET  "/run/sensord.sock"
#define SENSOR_MAX_DEVICES 16

/* === Sensor types / 传感器类型 === */
typedef enum {
    SENSOR_ACCEL = 0,        /* Accelerometer / 加速度计 */
    SENSOR_GYRO,             /* Gyroscope / 陀螺仪 */
    SENSOR_MAGNET,           /* Magnetometer / 磁力计 */
    SENSOR_LIGHT,            /* Ambient light / 环境光 */
    SENSOR_PROXIMITY,        /* Proximity / 接近 */
    SENSOR_PRESSURE,         /* Barometer / 气压计 */
    SENSOR_TEMPERATURE,      /* Temperature / 温度 */
    SENSOR_HUMIDITY,          /* Humidity / 湿度 */
    SENSOR_GRAVITY,          /* Gravity (computed) / 重力 */
    SENSOR_ROTATION,         /* Rotation vector (computed) / 旋转向量 */
    SENSOR_STEP_COUNTER,     /* Step counter / 计步器 */
} sensor_type_t;

/* === 3-axis sensor data / 三轴传感器数据 === */
typedef struct {
    float x;
    float y;
    float z;
} sensor_vec3_t;

/* === Sensor event / 传感器事件 === */
typedef struct {
    sensor_type_t type;
    uint64_t      timestamp;     /* nanoseconds / 纳秒 */
    union {
        sensor_vec3_t accel;     /* m/s² */
        sensor_vec3_t gyro;      /* rad/s */
        sensor_vec3_t magnet;    /* µT */
        float         light;     /* lux */
        float         proximity; /* cm, 0=near */
        float         pressure;  /* hPa */
        float         temperature; /* °C */
        float         humidity;  /* % */
        sensor_vec3_t gravity;   /* m/s² */
        sensor_vec3_t rotation;  /* quaternion xyz */
        uint32_t      steps;
    } data;
} sensor_event_t;

/* === Sensor device info / 传感器设备信息 === */
typedef struct {
    sensor_type_t type;
    char          name[64];
    char          iio_path[128];   /* /sys/bus/iio/devices/iioX */
    float         resolution;
    float         max_range;
    int           min_delay_us;    /* minimum sample period */
    bool          active;
    int           rate_hz;         /* current sample rate */
} sensor_info_t;

/* === Sensord instance / 传感器服务实例 === */
typedef struct {
    sensor_info_t  sensors[SENSOR_MAX_DEVICES];
    int            num_sensors;
    sensor_event_t last_events[SENSOR_MAX_DEVICES];
    bool           running;
} sensord_t;

/* === Sensor event callback / 传感器事件回调 === */
typedef void (*sensor_callback_t)(const sensor_event_t *event, void *userdata);

/* === Function declarations / 函数声明 === */

/* sensord.c - Core daemon */
int  sensord_init(sensord_t *s);
void sensord_run(sensord_t *s);
void sensord_shutdown(sensord_t *s);

/* enumerate.c - Sensor enumeration / 传感器枚举 */
int  sensor_enumerate(sensor_info_t *out, int max);
const char *sensor_type_name(sensor_type_t type);

/* reader.c - Sensor data reading / 传感器数据读取 */
int  sensor_enable(sensord_t *s, sensor_type_t type, int rate_hz);
int  sensor_disable(sensord_t *s, sensor_type_t type);
int  sensor_read(sensord_t *s, sensor_type_t type, sensor_event_t *event);
int  sensor_poll(sensord_t *s, sensor_event_t *events, int max, int timeout_ms);

/* fusion.c - Sensor fusion / 传感器融合 */
int  fusion_init(void);
void fusion_update(const sensor_event_t *accel, const sensor_event_t *gyro,
                   const sensor_event_t *magnet);
int  fusion_get_orientation(float *pitch, float *roll, float *yaw);
int  fusion_get_gravity(sensor_vec3_t *gravity);
int  fusion_get_rotation(sensor_vec3_t *rotation);

#endif /* SENSORD_H */
