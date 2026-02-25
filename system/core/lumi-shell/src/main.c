/**
 * main.c — LumiOS Shell (Community Edition)
 * Copyright 2026 Lumi Team. GPLv3
 *
 * Basic launcher with app grid and status bar.
 */

#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_APPS 64

struct lumi_shell {
    lumi_shell_config_t config;
    lumi_shell_app_entry_t apps[MAX_APPS];
    int app_count;
    bool running;
};

lumi_shell_t *lumi_shell_create(const lumi_shell_config_t *config) {
    if (!config) return NULL;
    lumi_shell_t *shell = calloc(1, sizeof(lumi_shell_t));
    if (!shell) return NULL;
    shell->config = *config;
    shell->app_count = 0;
    shell->running = false;
    fprintf(stderr, "[shell] Created %dx%d (community edition)\n",
            config->screen_width, config->screen_height);
    return shell;
}

void lumi_shell_destroy(lumi_shell_t *shell) {
    if (shell) {
        fprintf(stderr, "[shell] Destroyed\n");
        free(shell);
    }
}

void lumi_shell_add_app(lumi_shell_t *shell, const lumi_shell_app_entry_t *entry) {
    if (!shell || !entry || shell->app_count >= MAX_APPS) return;
    shell->apps[shell->app_count++] = *entry;
}

int lumi_shell_run(lumi_shell_t *shell) {
    if (!shell) return -1;
    shell->running = true;

    fprintf(stderr, "[shell] Launcher with %d apps\n", shell->app_count);
    for (int i = 0; i < shell->app_count; i++) {
        fprintf(stderr, "[shell]   [%d] %s (%s)\n", i,
                shell->apps[i].name ? shell->apps[i].name : "?",
                shell->apps[i].app_id ? shell->apps[i].app_id : "?");
    }

    fprintf(stderr, "[shell] Community edition running\n");
    return 0;
}

int main(int argc, char **argv) {
    lumi_shell_config_t config = {
        .screen_width = 1080,
        .screen_height = 2400,
        .wayland_display = "wayland-lumi",
        .wallpaper_path = NULL,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            fprintf(stderr, "lumi-shell (community edition)\n");
            return 0;
        }
    }

    const char *env_display = getenv("WAYLAND_DISPLAY");
    if (env_display) config.wayland_display = env_display;

    lumi_shell_t *shell = lumi_shell_create(&config);
    if (!shell) return 1;

    lumi_shell_add_app(shell, &(lumi_shell_app_entry_t){
        .app_id = "com.lumios.settings", .name = "Settings", .icon = NULL });
    lumi_shell_add_app(shell, &(lumi_shell_app_entry_t){
        .app_id = "com.lumios.files", .name = "Files", .icon = NULL });
    lumi_shell_add_app(shell, &(lumi_shell_app_entry_t){
        .app_id = "com.lumios.terminal", .name = "Terminal", .icon = NULL });

    int ret = lumi_shell_run(shell);
    lumi_shell_destroy(shell);
    return ret;
}
