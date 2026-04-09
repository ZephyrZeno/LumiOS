/**
 * widget.c — LumiOS Toolkit (Community Edition)
 * Copyright 2026 Lumi Team. GPLv3
 *
 * Basic widget tree, layout, and rendering stub.
 */

#include "lumi_toolkit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHILDREN 64

struct lumi_widget {
    lumi_widget_type_t type;
    char *text;
    float x, y, w, h;
    float padding;
    float spacing;
    uint32_t color;
    float opacity;
    float blur_radius;
    float corner_radius;
    float border_width;
    uint32_t border_color;
    float shadow_blur;
    uint32_t shadow_color;
    int motion_duration_ms;
    float spring_stiffness;
    float spring_damping;
    float spring_mass;
    lumi_layout_dir_t layout_dir;

    lumi_widget_t *children[MAX_CHILDREN];
    int child_count;

    lumi_widget_callback_t on_click;
    void *on_click_data;
};

lumi_widget_t *lumi_widget_create(lumi_widget_type_t type) {
    lumi_widget_t *w = calloc(1, sizeof(lumi_widget_t));
    if (!w) return NULL;
    w->type = type;
    w->color = 0xFFFFFFFF;
    w->opacity = 1.0f;
    w->spring_mass = 1.0f;
    return w;
}

void lumi_widget_destroy(lumi_widget_t *w) {
    if (!w) return;
    for (int i = 0; i < w->child_count; i++)
        lumi_widget_destroy(w->children[i]);
    free(w->text);
    free(w);
}

void lumi_widget_add_child(lumi_widget_t *parent, lumi_widget_t *child) {
    if (!parent || !child || parent->child_count >= MAX_CHILDREN) return;
    parent->children[parent->child_count++] = child;
}

void lumi_widget_set_text(lumi_widget_t *w, const char *text) {
    if (!w) return;
    free(w->text);
    w->text = text ? strdup(text) : NULL;
}

void lumi_widget_set_size(lumi_widget_t *w, float width, float height) {
    if (!w) return;
    w->w = width;
    w->h = height;
}

void lumi_widget_set_layout(lumi_widget_t *w, lumi_layout_dir_t dir) {
    if (w) w->layout_dir = dir;
}

void lumi_widget_set_padding(lumi_widget_t *w, float pad) {
    if (w) w->padding = pad;
}

void lumi_widget_set_spacing(lumi_widget_t *w, float spacing) {
    if (w) w->spacing = spacing;
}

void lumi_widget_set_color(lumi_widget_t *w, uint32_t rgba) {
    if (w) w->color = rgba;
}

void lumi_widget_set_opacity(lumi_widget_t *w, float opacity) {
    if (!w) return;
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    w->opacity = opacity;
}

void lumi_widget_set_blur(lumi_widget_t *w, float blur_radius) {
    if (w) w->blur_radius = blur_radius < 0.0f ? 0.0f : blur_radius;
}

void lumi_widget_set_corner_radius(lumi_widget_t *w, float radius) {
    if (w) w->corner_radius = radius < 0.0f ? 0.0f : radius;
}

void lumi_widget_set_border(lumi_widget_t *w, float width, uint32_t color) {
    if (!w) return;
    w->border_width = width < 0.0f ? 0.0f : width;
    w->border_color = color;
}

void lumi_widget_set_shadow(lumi_widget_t *w, float blur_radius, uint32_t color) {
    if (!w) return;
    w->shadow_blur = blur_radius < 0.0f ? 0.0f : blur_radius;
    w->shadow_color = color;
}

void lumi_widget_apply_glass(lumi_widget_t *w, lumi_glass_variant_t variant) {
    static const struct {
        float blur_radius;
        float opacity;
        float corner_radius;
        uint32_t tint;
        uint32_t border;
        float shadow_blur;
        uint32_t shadow;
    } presets[] = {
        { 12.0f, 0.45f, 16.0f, 0xFFFFFF72, 0xFFFFFF22, 14.0f, 0x0F172A18 },
        { 20.0f, 0.60f, 18.0f, 0xFFFFFF99, 0xFFFFFF33, 18.0f, 0x0F172A20 },
        { 30.0f, 0.72f, 22.0f, 0xFFFFFFB8, 0xFFFFFF40, 24.0f, 0x0F172A24 },
        { 40.0f, 0.85f, 24.0f, 0xFFFFFFD8, 0xFFFFFF4A, 32.0f, 0x0F172A28 },
        { 30.0f, 0.65f, 22.0f, 0xCFE2FFB0, 0xFFFFFF46, 28.0f, 0x0F172A24 },
    };
    int index = (variant < LUMI_GLASS_ULTRA_THIN || variant > LUMI_GLASS_CHROMATIC)
        ? LUMI_GLASS_REGULAR : variant;
    if (!w) return;
    lumi_widget_set_color(w, presets[index].tint);
    lumi_widget_set_opacity(w, presets[index].opacity);
    lumi_widget_set_blur(w, presets[index].blur_radius);
    lumi_widget_set_corner_radius(w, presets[index].corner_radius);
    lumi_widget_set_border(w, 1.0f, presets[index].border);
    lumi_widget_set_shadow(w, presets[index].shadow_blur, presets[index].shadow);
}

void lumi_widget_apply_motion(lumi_widget_t *w, lumi_motion_preset_t preset) {
    static const struct {
        int duration_ms;
        float stiffness;
        float damping;
        float mass;
    } presets[] = {
        { 350, 400.0f, 28.0f, 1.0f },
        { 400, 200.0f, 20.0f, 1.0f },
        { 400, 300.0f, 15.0f, 1.0f },
        { 300, 500.0f, 30.0f, 1.0f },
    };
    int index = (preset < LUMI_MOTION_RESPONSIVE || preset > LUMI_MOTION_STIFF)
        ? LUMI_MOTION_RESPONSIVE : preset;
    if (!w) return;
    w->motion_duration_ms = presets[index].duration_ms;
    w->spring_stiffness = presets[index].stiffness;
    w->spring_damping = presets[index].damping;
    w->spring_mass = presets[index].mass;
}

void lumi_widget_on_click(lumi_widget_t *w, lumi_widget_callback_t cb, void *ud) {
    if (!w) return;
    w->on_click = cb;
    w->on_click_data = ud;
}

void lumi_widget_layout(lumi_widget_t *root, float width, float height) {
    if (!root) return;
    root->x = 0;
    root->y = 0;
    if (root->w == 0) root->w = width;
    if (root->h == 0) root->h = height;

    float offset = root->padding;
    for (int i = 0; i < root->child_count; i++) {
        lumi_widget_t *c = root->children[i];
        if (root->layout_dir == LUMI_LAYOUT_COLUMN) {
            c->x = root->x + root->padding;
            c->y = root->y + offset;
            if (c->w == 0) c->w = root->w - root->padding * 2;
            if (c->h == 0) c->h = 40.0f;
            offset += c->h + root->spacing;
        } else {
            c->x = root->x + offset;
            c->y = root->y + root->padding;
            if (c->w == 0) c->w = 100.0f;
            if (c->h == 0) c->h = root->h - root->padding * 2;
            offset += c->w + root->spacing;
        }
        lumi_widget_layout(c, c->w, c->h);
    }
}

void lumi_widget_render(lumi_widget_t *root) {
    (void)root;
    /* Stub — real implementation calls lumi_render_* functions */
}
