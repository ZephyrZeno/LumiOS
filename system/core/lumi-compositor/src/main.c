/**
 * main.c — LumiOS Compositor entry point (Open-Source Stub)
 * Copyright 2026 Lumi Team. GPLv3
 *
 * Minimal wlroots-based compositor. No Liquid Glass effects.
 */

#include "compositor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct lumi_compositor {
    lumi_compositor_config_t config;
    bool running;
};

lumi_compositor_t *lumi_compositor_create(const lumi_compositor_config_t *config) {
    if (!config) return NULL;
    lumi_compositor_t *comp = calloc(1, sizeof(lumi_compositor_t));
    if (!comp) return NULL;
    comp->config = *config;
    comp->running = false;

    fprintf(stderr, "[compositor] Created %dx%d (open-source stub)\n",
            config->width, config->height);
    fprintf(stderr, "[compositor] Note: Liquid Glass effects require LumiShell\n");
    return comp;
}

void lumi_compositor_destroy(lumi_compositor_t *comp) {
    if (comp) {
        fprintf(stderr, "[compositor] Destroyed\n");
        free(comp);
    }
}

int lumi_compositor_run(lumi_compositor_t *comp) {
    if (!comp) return -1;
    comp->running = true;
    fprintf(stderr, "[compositor] Running on socket: %s\n",
            comp->config.socket_name ? comp->config.socket_name : "wayland-0");

    /*
     * TODO: Real implementation would initialize wlroots here:
     *   - wlr_backend_autocreate()
     *   - wlr_renderer_autocreate()
     *   - wlr_compositor_create()
     *   - wl_display_run()
     *
     * This stub just prints a message and returns.
     */

    fprintf(stderr, "[compositor] Stub compositor ready (no wlroots linked)\n");
    return 0;
}

static void usage(void) {
    fprintf(stderr,
        "lumi-compositor (open-source stub)\n"
        "Usage: lumi-compositor [-s socket_name] [-w width] [-h height]\n");
}

int main(int argc, char **argv) {
    lumi_compositor_config_t config = {
        .width = 1080, .height = 2400,
        .socket_name = "wayland-lumi",
        .headless = false,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            config.socket_name = argv[++i];
        else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc)
            config.width = atoi(argv[++i]);
        else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc)
            config.height = atoi(argv[++i]);
        else if (strcmp(argv[i], "--headless") == 0)
            config.headless = true;
        else if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        }
    }

    lumi_compositor_t *comp = lumi_compositor_create(&config);
    if (!comp) return 1;
    int ret = lumi_compositor_run(comp);
    lumi_compositor_destroy(comp);
    return ret;
}
