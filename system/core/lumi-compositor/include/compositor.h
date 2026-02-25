/**
 * compositor.h — LumiOS Wayland Compositor (Open-Source Stub)
 * Copyright 2026 Lumi Team. GPLv3
 *
 * Basic Wayland compositor without advanced visual effects.
 * For the full version with Liquid Glass, use LumiShell.
 */

#ifndef LUMI_COMPOSITOR_H
#define LUMI_COMPOSITOR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct lumi_compositor lumi_compositor_t;

typedef struct {
    int width;
    int height;
    const char *socket_name;
    bool headless;
} lumi_compositor_config_t;

lumi_compositor_t *lumi_compositor_create(const lumi_compositor_config_t *config);
void               lumi_compositor_destroy(lumi_compositor_t *comp);
int                lumi_compositor_run(lumi_compositor_t *comp);

#endif /* LUMI_COMPOSITOR_H */
