/**
 * lumi_toolkit.h — LumiOS UI Toolkit (Community Edition)
 * Copyright 2026 Lumi Team. GPLv3
 *
 * Basic Flexbox layout + widget rendering.
 */

#ifndef LUMI_OS_TOOLKIT_H
#define LUMI_OS_TOOLKIT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct lumi_widget lumi_widget_t;

typedef enum {
    LUMI_WIDGET_BOX,
    LUMI_WIDGET_LABEL,
    LUMI_WIDGET_BUTTON,
    LUMI_WIDGET_IMAGE,
} lumi_widget_type_t;

typedef enum {
    LUMI_LAYOUT_ROW,
    LUMI_LAYOUT_COLUMN,
} lumi_layout_dir_t;

typedef void (*lumi_widget_callback_t)(lumi_widget_t *widget, void *userdata);

/* Creation */
lumi_widget_t *lumi_widget_create(lumi_widget_type_t type);
void            lumi_widget_destroy(lumi_widget_t *w);

/* Tree */
void lumi_widget_add_child(lumi_widget_t *parent, lumi_widget_t *child);

/* Properties */
void lumi_widget_set_text(lumi_widget_t *w, const char *text);
void lumi_widget_set_size(lumi_widget_t *w, float width, float height);
void lumi_widget_set_layout(lumi_widget_t *w, lumi_layout_dir_t dir);
void lumi_widget_set_padding(lumi_widget_t *w, float pad);
void lumi_widget_set_spacing(lumi_widget_t *w, float spacing);
void lumi_widget_set_color(lumi_widget_t *w, uint32_t rgba);
void lumi_widget_on_click(lumi_widget_t *w, lumi_widget_callback_t cb, void *ud);

/* Layout */
void lumi_widget_layout(lumi_widget_t *root, float width, float height);

/* Render (stub — real impl uses lumi-render) */
void lumi_widget_render(lumi_widget_t *root);

#endif /* LUMI_OS_TOOLKIT_H */
