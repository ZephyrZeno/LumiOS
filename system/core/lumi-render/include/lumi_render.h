/**
 * lumi_render.h — LumiOS Render Abstraction (Community Edition)
 * Copyright 2026 Lumi Team. GPLv3
 *
 * Cairo software rendering backend only.
 */

#ifndef LUMI_RENDER_H
#define LUMI_RENDER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct lumi_renderer lumi_renderer_t;

typedef enum {
    LUMI_BACKEND_CAIRO = 0,
} lumi_render_backend_t;

typedef struct {
    float x, y, w, h;
} lumi_rect_t;

typedef struct {
    uint8_t r, g, b, a;
} lumi_color_t;

lumi_renderer_t *lumi_renderer_create(lumi_render_backend_t backend);
void              lumi_renderer_destroy(lumi_renderer_t *r);

void lumi_render_clear(lumi_renderer_t *r, lumi_color_t color);
void lumi_render_rect(lumi_renderer_t *r, lumi_rect_t rect, lumi_color_t color);
void lumi_render_rounded_rect(lumi_renderer_t *r, lumi_rect_t rect,
                               float radius, lumi_color_t color);
void lumi_render_text(lumi_renderer_t *r, float x, float y,
                      const char *text, float size, lumi_color_t color);
void lumi_render_line(lumi_renderer_t *r, float x1, float y1,
                      float x2, float y2, float width, lumi_color_t color);

#endif /* LUMI_RENDER_H */
