/*
 * volume.c - Volume control / 音量控制
 *
 * Per-stream volume management with persistent state.
 * 每流音量管理，带持久化状态。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "audiod.h"

#define VOLUME_FILE "/var/lib/audiod/volume.conf"

static volume_state_t g_vol;

/* Stream names for config / 流名称 */
static const char *stream_names[] = {
    "media", "ringtone", "alarm", "notification", "call", "system"
};

int volume_init(void)
{
    /* Defaults / 默认值 */
    g_vol.level[STREAM_MEDIA]        = 0.6f;
    g_vol.level[STREAM_RINGTONE]     = 0.8f;
    g_vol.level[STREAM_ALARM]        = 0.9f;
    g_vol.level[STREAM_NOTIFICATION] = 0.7f;
    g_vol.level[STREAM_CALL]         = 0.8f;
    g_vol.level[STREAM_SYSTEM]       = 0.5f;
    g_vol.active_output = AUDIO_SPEAKER;

    for (int i = 0; i < 6; i++) g_vol.muted[i] = false;

    /* Load from file / 从文件加载 */
    FILE *fp = fopen(VOLUME_FILE, "r");
    if (fp) {
        char line[128];
        while (fgets(line, sizeof(line), fp)) {
            char key[32]; float val;
            if (sscanf(line, "%31s = %f", key, &val) == 2) {
                for (int i = 0; i < 6; i++) {
                    if (strcmp(key, stream_names[i]) == 0) {
                        g_vol.level[i] = val;
                        break;
                    }
                }
            }
        }
        fclose(fp);
        fprintf(stderr, "[audiod] volume: loaded from %s\n", VOLUME_FILE);
    }

    return 0;
}

float volume_get(stream_type_t stream)
{
    if (stream < 0 || stream > 5) return 0;
    return g_vol.muted[stream] ? 0.0f : g_vol.level[stream];
}

int volume_set(stream_type_t stream, float level)
{
    if (stream < 0 || stream > 5) return -1;
    if (level < 0) level = 0;
    if (level > 1) level = 1;

    g_vol.level[stream] = level;
    fprintf(stderr, "[audiod] volume: %s = %.2f\n", stream_names[stream], level);

    /* TODO: apply to PipeWire / TODO: 应用到 PipeWire */
    return 0;
}

bool volume_get_mute(stream_type_t stream)
{
    if (stream < 0 || stream > 5) return false;
    return g_vol.muted[stream];
}

int volume_set_mute(stream_type_t stream, bool mute)
{
    if (stream < 0 || stream > 5) return -1;
    g_vol.muted[stream] = mute;
    fprintf(stderr, "[audiod] volume: %s %s\n",
            stream_names[stream], mute ? "muted" : "unmuted");
    return 0;
}

void volume_save(void)
{
    FILE *fp = fopen(VOLUME_FILE, "w");
    if (!fp) {
        fprintf(stderr, "[audiod] volume: failed to save: %s\n", strerror(errno));
        return;
    }

    fprintf(fp, "# LumiOS audiod volume configuration\n");
    for (int i = 0; i < 6; i++) {
        fprintf(fp, "%s = %.2f\n", stream_names[i], g_vol.level[i]);
    }
    fclose(fp);
    fprintf(stderr, "[audiod] volume: saved to %s\n", VOLUME_FILE);
}
