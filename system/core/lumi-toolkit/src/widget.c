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
