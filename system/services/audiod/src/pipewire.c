/*
 * pipewire.c - PipeWire integration / PipeWire 集成
 *
 * Interfaces with PipeWire for audio output routing.
 * 通过 PipeWire 进行音频输出路由。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include "audiod.h"

static audio_output_t g_active = AUDIO_SPEAKER;

int pw_init(void)
{
    /* TODO: connect to PipeWire daemon via libpipewire */
    /* TODO: 通过 libpipewire 连接 PipeWire 守护进程 */
    fprintf(stderr, "[audiod] pipewire: initialized (stub)\n");
    return 0;
}

int pw_set_output(audio_output_t output)
{
    g_active = output;
    const char *names[] = {"speaker","headphone","bluetooth","hdmi","usb"};
    fprintf(stderr, "[audiod] pipewire: output set to %s\n", names[output]);
    return 0;
}

int pw_get_outputs(audio_output_t *out, int max)
{
    if (max < 1) return 0;
    out[0] = AUDIO_SPEAKER;
    int count = 1;
    /* TODO: enumerate PipeWire sinks / TODO: 枚举 PipeWire sink */
    return count;
}

void pw_shutdown(void)
{
    fprintf(stderr, "[audiod] pipewire: shutdown\n");
}
