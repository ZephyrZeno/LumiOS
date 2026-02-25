/**
 * shell.h — LumiOS Mobile Shell (Open-Source Stub)
 * Copyright 2026 Lumi Team. GPLv3
 *
 * Basic launcher and status bar. No Liquid Glass effects.
 * For the full version, use LumiShell.
 */

#ifndef LUMI_SHELL_H
#define LUMI_SHELL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct lumi_shell lumi_shell_t;

typedef struct {
    int screen_width;
    int screen_height;
    const char *wayland_display;
    const char *wallpaper_path;
} lumi_shell_config_t;

lumi_shell_t *lumi_shell_create(const lumi_shell_config_t *config);
void           lumi_shell_destroy(lumi_shell_t *shell);
int            lumi_shell_run(lumi_shell_t *shell);

/* App launcher */
typedef struct {
    const char *app_id;
    const char *name;
    const char *icon;
} lumi_shell_app_entry_t;

void lumi_shell_add_app(lumi_shell_t *shell, const lumi_shell_app_entry_t *entry);

#endif /* LUMI_SHELL_H */
