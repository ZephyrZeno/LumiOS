/*
 * audiod.h - LumiOS Audio Service / 音频服务
 *
 * Manages audio routing, volume control, and PipeWire integration.
 * 管理音频路由、音量控制和 PipeWire 集成。
 */

#ifndef AUDIOD_H
#define AUDIOD_H

#include <stdbool.h>
#include <stdint.h>

#define AUDIOD_VERSION "0.1.0"
#define AUDIOD_SOCKET  "/run/audiod.sock"

/* === Audio output type / 音频输出类型 === */
typedef enum {
    AUDIO_SPEAKER = 0,
    AUDIO_HEADPHONE,
    AUDIO_BLUETOOTH,
    AUDIO_HDMI,
    AUDIO_USB,
} audio_output_t;

/* === Audio stream type / 音频流类型 === */
typedef enum {
    STREAM_MEDIA = 0,
    STREAM_RINGTONE,
    STREAM_ALARM,
    STREAM_NOTIFICATION,
    STREAM_CALL,
    STREAM_SYSTEM,
} stream_type_t;

/* === Volume state / 音量状态 === */
typedef struct {
    float level[6];        /* Per-stream volume 0.0-1.0 / 每流音量 */
    bool  muted[6];
    audio_output_t active_output;
} volume_state_t;

/* === Function declarations / 函数声明 === */

/* audiod.c - Core daemon / 核心守护进程 */
int  audiod_init(void);
void audiod_run(void);
void audiod_shutdown(void);

/* pipewire.c - PipeWire integration / PipeWire 集成 */
int  pw_init(void);
int  pw_set_output(audio_output_t output);
int  pw_get_outputs(audio_output_t *out, int max);
void pw_shutdown(void);

/* volume.c - Volume control / 音量控制 */
int   volume_init(void);
float volume_get(stream_type_t stream);
int   volume_set(stream_type_t stream, float level);
bool  volume_get_mute(stream_type_t stream);
int   volume_set_mute(stream_type_t stream, bool mute);
void  volume_save(void);

#endif /* AUDIOD_H */
