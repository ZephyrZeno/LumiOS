/**
 * render.c — LumiOS Render (Community Edition)
 * Copyright 2026 Lumi Team. GPLv3
 *
 * Stub implementation — logs draw calls.
 * Real implementation links against Cairo.
 */

#include "lumi_render.h"
#include <stdio.h>
#include <stdlib.h>

struct lumi_renderer {
    lumi_render_backend_t backend;
};

lumi_renderer_t *lumi_renderer_create(lumi_render_backend_t backend) {
    lumi_renderer_t *r = calloc(1, sizeof(lumi_renderer_t));
    if (!r) return NULL;
    r->backend = backend;
    fprintf(stderr, "[render] Created (community edition, cairo-stub)\n");
    return r;
}

void lumi_renderer_destroy(lumi_renderer_t *r) {
    free(r);
}

void lumi_render_clear(lumi_renderer_t *r, lumi_color_t c) {
    (void)r;
    (void)c;
}

void lumi_render_rect(lumi_renderer_t *r, lumi_rect_t rect, lumi_color_t c) {
    (void)r; (void)rect; (void)c;
}

void lumi_render_rounded_rect(lumi_renderer_t *r, lumi_rect_t rect,
                               float radius, lumi_color_t c) {
    (void)r; (void)rect; (void)radius; (void)c;
}

void lumi_render_text(lumi_renderer_t *r, float x, float y,
                      const char *text, float size, lumi_color_t c) {
    (void)r; (void)x; (void)y; (void)text; (void)size; (void)c;
}

void lumi_render_line(lumi_renderer_t *r, float x1, float y1,
                      float x2, float y2, float width, lumi_color_t c) {
    (void)r; (void)x1; (void)y1; (void)x2; (void)y2; (void)width; (void)c;
}
