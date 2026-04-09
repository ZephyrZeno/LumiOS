#include "shell_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_string(char *dst, size_t len, const char *src) {
    size_t n;

    if (!dst || len == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    n = strlen(src);
    if (n >= len) {
        n = len - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static bool string_equals(const char *lhs, const char *rhs) {
    const char *left = lhs ? lhs : "";
    const char *right = rhs ? rhs : "";

    return strcmp(left, right) == 0;
}

static int clamp_percent(int value, int fallback) {
    if (value < 0) {
        return fallback;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

static const shell_app_slot_t *find_app(const lumi_shell_t *shell, const char *app_id) {
    if (!shell || !app_id || app_id[0] == '\0') {
        return NULL;
    }

    for (int i = 0; i < shell->app_count; i++) {
        if (strcmp(shell->apps[i].app_id, app_id) == 0) {
            return &shell->apps[i];
        }
    }

    return NULL;
}

static const char *surface_name(const lumi_shell_t *shell) {
    const char *panel;

    if (!shell) {
        return "home";
    }
    if (shell->config.locked) {
        return "lockscreen";
    }

    panel = shell->config.open_panel;
    if (!panel || panel[0] == '\0') {
        return shell->config.active_app && shell->config.active_app[0] != '\0' ? "app" : "home";
    }
    return panel;
}

static void print_status_bar(const lumi_shell_t *shell) {
    const char *time_text = shell->config.time_override ? shell->config.time_override : "09:41";

    fprintf(stderr,
            "[shell] Status bar: %s | Wi-Fi %s | Bluetooth %s | Mobile %s | DND %s | Battery %d%% | Theme %s\n",
            time_text,
            shell->config.wifi_enabled ? "on" : "off",
            shell->config.bluetooth_enabled ? "on" : "off",
            shell->config.mobile_data_enabled ? "on" : "off",
            shell->config.do_not_disturb ? "on" : "off",
            clamp_percent(shell->config.battery_level, 86),
            shell->config.dark_mode ? "dark" : "light");
}

static void print_surface_summary(const lumi_shell_t *shell) {
    const char *surface = surface_name(shell);

    fprintf(stderr, "[shell] Surface: %s\n", surface);

    if (strcmp(surface, "quick-settings") == 0) {
        fprintf(stderr,
                "[shell] Quick settings: brightness=%d%% volume=%d%% night-light=%s saver=%s auto-brightness=%s motion=%s transparency=%s\n",
                clamp_percent(shell->config.brightness_level, 72),
                clamp_percent(shell->config.volume_level, 28),
                shell->config.night_light ? "on" : "off",
                shell->config.battery_saver ? "on" : "off",
                shell->config.auto_brightness ? "on" : "off",
                shell->config.reduce_motion ? "reduced" : "full",
                shell->config.reduce_transparency ? "reduced" : "full");
        return;
    }

    if (strcmp(surface, "notifications") == 0) {
        fprintf(stderr, "[shell] Notification center: %d item(s)\n", shell->notification_count);
        return;
    }

    if (strcmp(surface, "recents") == 0) {
        fprintf(stderr, "[shell] Recents: Settings -> Files -> Browser\n");
        return;
    }

    if (strcmp(surface, "launcher") == 0) {
        fprintf(stderr, "[shell] Launcher grid prepared with %d app(s)\n", shell->app_count);
        return;
    }

    if (strcmp(surface, "lockscreen") == 0) {
        fprintf(stderr, "[shell] Lockscreen: notifications=%d wallpaper=%s\n",
                shell->notification_count,
                shell->config.wallpaper_path && shell->config.wallpaper_path[0] != '\0'
                    ? shell->config.wallpaper_path
                    : "default gradient");
    }
}

static void print_app_context(const lumi_shell_t *shell) {
    const shell_app_slot_t *active = find_app(shell, shell->config.active_app);

    if (active) {
        fprintf(stderr, "[shell] Active app: %s (%s v%s)\n",
                active->name,
                active->app_id,
                active->version[0] != '\0' ? active->version : "unknown");
        return;
    }

    fprintf(stderr, "[shell] Active app: none\n");
}

static void print_dock(const lumi_shell_t *shell) {
    const int dock_count = shell->app_count < 4 ? shell->app_count : 4;

    fprintf(stderr, "[shell] Dock:");
    for (int i = 0; i < dock_count; i++) {
        fprintf(stderr, " %s", shell->apps[i].name);
        if (i + 1 < dock_count) {
            fputc(',', stderr);
        }
    }
    fputc('\n', stderr);
}

static void print_notifications(const lumi_shell_t *shell) {
    if (shell->notification_count == 0) {
        fprintf(stderr, "[shell] Notifications: clear\n");
        return;
    }

    fprintf(stderr, "[shell] Notifications (%d):\n", shell->notification_count);
    for (int i = 0; i < shell->notification_count; i++) {
        const shell_notification_slot_t *notification = &shell->notifications[i];
        fprintf(stderr, "[shell]   [%d] %s", i, notification->title[0] ? notification->title : "Lumi");
        if (notification->source[0] != '\0') {
            fprintf(stderr, " <%s>", notification->source);
        }
        if (notification->silent) {
            fprintf(stderr, " [silent]");
        }
        if (notification->body[0] != '\0') {
            fprintf(stderr, " - %s", notification->body);
        }
        fputc('\n', stderr);
    }
}

static void print_live_notification(const lumi_shell_notification_t *notification) {
    if (!notification) {
        return;
    }

    fprintf(stderr, "[shell] Live notification: %s",
            notification->title && notification->title[0] != '\0'
                ? notification->title
                : "Lumi");
    if (notification->source && notification->source[0] != '\0') {
        fprintf(stderr, " <%s>", notification->source);
    }
    if (notification->body && notification->body[0] != '\0') {
        fprintf(stderr, " - %s", notification->body);
    }
    fputc('\n', stderr);
}

void lumi_shell_refresh_runtime_state(lumi_shell_t *shell,
                                      lumi_shell_notification_t *shared_notification,
                                      bool *has_shared_notification,
                                      lumi_shell_notification_t *daemon_notifications,
                                      int *daemon_notification_count) {
    lumi_shell_config_t refreshed_config;
    bool shared_changed = false;
    lumi_shell_notification_t local_shared_notification = {0};
    lumi_shell_notification_t local_daemon_notifications[MAX_NOTIFICATIONS] = {0};
    bool local_has_shared_notification = false;
    int local_daemon_notification_count = 0;

    if (!shell || !shell->shared_state) {
        if (has_shared_notification) {
            *has_shared_notification = false;
        }
        if (daemon_notification_count) {
            *daemon_notification_count = 0;
        }
        return;
    }

    refreshed_config = shell->base_config;
    shared_changed = lumi_shell_refresh_shared_state(&refreshed_config,
                                                     &shell->overrides,
                                                     shell->shared_state,
                                                     &local_shared_notification,
                                                     &local_has_shared_notification);
    if (shared_changed) {
        shell->config = refreshed_config;
    }

    lumi_shell_apply_daemon_events(&shell->config,
                                   &shell->overrides,
                                   shell->shared_state,
                                   local_daemon_notifications,
                                   &local_daemon_notification_count,
                                   MAX_NOTIFICATIONS,
                                   &shell->daemon_last_seen_event_id);

    if (shared_notification) {
        *shared_notification = local_shared_notification;
    }
    if (has_shared_notification) {
        *has_shared_notification = shared_changed && local_has_shared_notification;
    }
    if (daemon_notification_count) {
        *daemon_notification_count = local_daemon_notification_count;
    }
    if (daemon_notifications && local_daemon_notification_count > 0) {
        for (int i = 0; i < local_daemon_notification_count; i++) {
            daemon_notifications[i] = local_daemon_notifications[i];
        }
    }
}

static void poll_runtime_state(lumi_shell_t *shell) {
    lumi_shell_notification_t shared_notification = {0};
    lumi_shell_notification_t daemon_notifications[MAX_NOTIFICATIONS];
    char previous_status[MAX_STATUS_LEN];
    char previous_recent[MAX_BODY_LEN];
    char previous_active_app[MAX_APP_ID_LEN];
    char previous_workspace[MAX_PATH_LEN];
    bool previous_wifi;
    bool previous_bluetooth;
    bool previous_mobile_data;
    bool previous_dnd;
    bool previous_dark_mode;
    bool previous_night_light;
    bool previous_battery_saver;
    bool previous_auto_brightness;
    int previous_battery_level;
    int previous_brightness_level;
    int previous_volume_level;
    bool previous_connected;
    bool has_shared_notification = false;
    int daemon_notification_count = 0;
    bool status_bar_changed = false;
    bool quick_settings_changed = false;

    if (!shell || !shell->shared_state) {
        return;
    }

    copy_string(previous_status,
                sizeof(previous_status),
                shell->config.status_line ? shell->config.status_line : "");
    copy_string(previous_recent,
                sizeof(previous_recent),
                shell->config.recent_action ? shell->config.recent_action : "");
    copy_string(previous_active_app,
                sizeof(previous_active_app),
                shell->config.active_app ? shell->config.active_app : "");
    copy_string(previous_workspace,
                sizeof(previous_workspace),
                shell->config.workspace_path ? shell->config.workspace_path : "");
    previous_wifi = shell->config.wifi_enabled;
    previous_bluetooth = shell->config.bluetooth_enabled;
    previous_mobile_data = shell->config.mobile_data_enabled;
    previous_dnd = shell->config.do_not_disturb;
    previous_dark_mode = shell->config.dark_mode;
    previous_night_light = shell->config.night_light;
    previous_battery_saver = shell->config.battery_saver;
    previous_auto_brightness = shell->config.auto_brightness;
    previous_battery_level = shell->config.battery_level;
    previous_brightness_level = shell->config.brightness_level;
    previous_volume_level = shell->config.volume_level;
    previous_connected = shell->config.daemon_connected;

    daemon_notifications[0] = (lumi_shell_notification_t){0};
    lumi_shell_refresh_runtime_state(shell,
                                     &shared_notification,
                                     &has_shared_notification,
                                     daemon_notifications,
                                     &daemon_notification_count);

    if (shell->config.lumid_socket_path && shell->config.lumid_socket_path[0] != '\0' &&
        previous_connected != shell->config.daemon_connected) {
        fprintf(stderr,
                "[shell] Event bridge %s: %s\n",
                shell->config.daemon_connected ? "connected" : "offline",
                shell->config.lumid_socket_path);
    }

    if (!string_equals(previous_status, shell->config.status_line)) {
        fprintf(stderr, "[shell] Session status updated: %s\n",
                shell->config.status_line && shell->config.status_line[0] != '\0'
                    ? shell->config.status_line
                    : "Session ready");
    }

    if (!string_equals(previous_workspace, shell->config.workspace_path)) {
        fprintf(stderr, "[shell] Shared workspace updated: %s\n",
                shell->config.workspace_path && shell->config.workspace_path[0] != '\0'
                    ? shell->config.workspace_path
                    : "none");
    }

    if (!string_equals(previous_recent, shell->config.recent_action) &&
        shell->config.recent_action && shell->config.recent_action[0] != '\0') {
        fprintf(stderr, "[shell] Recent action updated: %s\n", shell->config.recent_action);
    }

    if (!string_equals(previous_active_app, shell->config.active_app)) {
        const shell_app_slot_t *active = find_app(shell, shell->config.active_app);

        if (active) {
            fprintf(stderr,
                    "[shell] Active app switched: %s (%s v%s)\n",
                    active->name,
                    active->app_id,
                    active->version[0] != '\0' ? active->version : "unknown");
        } else if (shell->config.active_app && shell->config.active_app[0] != '\0') {
            fprintf(stderr, "[shell] Active app switched: %s\n", shell->config.active_app);
        } else {
            fprintf(stderr, "[shell] Active app switched: none\n");
        }
    }

    status_bar_changed =
        shell->config.wifi_enabled != previous_wifi ||
        shell->config.bluetooth_enabled != previous_bluetooth ||
        shell->config.mobile_data_enabled != previous_mobile_data ||
        shell->config.do_not_disturb != previous_dnd ||
        shell->config.battery_level != previous_battery_level ||
        shell->config.dark_mode != previous_dark_mode;

    quick_settings_changed =
        shell->config.brightness_level != previous_brightness_level ||
        shell->config.volume_level != previous_volume_level ||
        shell->config.night_light != previous_night_light ||
        shell->config.battery_saver != previous_battery_saver ||
        shell->config.auto_brightness != previous_auto_brightness;

    if (status_bar_changed) {
        fprintf(stderr, "[shell] Status bar updated\n");
        print_status_bar(shell);
    }

    if (quick_settings_changed) {
        fprintf(stderr,
                "[shell] Quick settings updated: brightness=%d%% volume=%d%% night-light=%s saver=%s auto-brightness=%s\n",
                clamp_percent(shell->config.brightness_level, 72),
                clamp_percent(shell->config.volume_level, 28),
                shell->config.night_light ? "on" : "off",
                shell->config.battery_saver ? "on" : "off",
                shell->config.auto_brightness ? "on" : "off");
    }

    if (!shell->config.daemon_connected &&
        has_shared_notification &&
        !string_equals(previous_recent, shell->config.recent_action)) {
        lumi_shell_push_notification(shell, &shared_notification);
        print_live_notification(&shared_notification);
    }

    for (int i = 0; i < daemon_notification_count; i++) {
        lumi_shell_push_notification(shell, &daemon_notifications[i]);
        print_live_notification(&daemon_notifications[i]);
    }
}

static void print_shared_session(const lumi_shell_t *shell) {
    fprintf(stderr, "[shell] Session status: %s\n",
            shell->config.status_line && shell->config.status_line[0] != '\0'
                ? shell->config.status_line
                : "Session ready");

    if (shell->config.workspace_path && shell->config.workspace_path[0] != '\0') {
        fprintf(stderr, "[shell] Shared workspace: %s\n", shell->config.workspace_path);
    } else {
        fprintf(stderr, "[shell] Shared workspace: none\n");
    }

    if (shell->config.recent_action && shell->config.recent_action[0] != '\0') {
        fprintf(stderr, "[shell] Recent action: %s\n", shell->config.recent_action);
    }

    if (shell->config.storage_path && shell->config.storage_path[0] != '\0') {
        fprintf(stderr, "[shell] Shared storage: %s\n", shell->config.storage_path);
    }

    if (shell->config.lumid_socket_path && shell->config.lumid_socket_path[0] != '\0') {
        fprintf(stderr,
                "[shell] Event bridge: %s (%s, %d buffered event%s)\n",
                shell->config.lumid_socket_path,
                shell->config.daemon_connected ? "connected" : "offline",
                shell->config.daemon_event_count,
                shell->config.daemon_event_count == 1 ? "" : "s");
        if (shell->config.daemon_connected &&
            shell->config.daemon_last_event_type &&
            shell->config.daemon_last_event_type[0] != '\0') {
            fprintf(stderr,
                    "[shell] Last daemon event: %s from %s\n",
                    shell->config.daemon_last_event_type,
                    shell->config.daemon_last_event_source &&
                        shell->config.daemon_last_event_source[0] != '\0'
                        ? shell->config.daemon_last_event_source
                        : "lumid");
        }
    }
}

lumi_shell_t *lumi_shell_create(const lumi_shell_config_t *config) {
    lumi_shell_t *shell;

    if (!config) {
        return NULL;
    }

    shell = calloc(1, sizeof(lumi_shell_t));
    if (!shell) {
        return NULL;
    }

    shell->config = *config;
    shell->base_config = *config;
    memset(&shell->overrides, 0, sizeof(shell->overrides));
    shell->shared_state = NULL;
    shell->app_count = 0;
    shell->notification_count = 0;
    shell->daemon_last_seen_event_id = 0;
    shell->running = false;

    fprintf(stderr, "[shell] Created %dx%d (community edition)\n",
            config->screen_width, config->screen_height);
    return shell;
}

void lumi_shell_destroy(lumi_shell_t *shell) {
    if (!shell) {
        return;
    }

    fprintf(stderr, "[shell] Destroyed\n");
    free(shell);
}

void lumi_shell_push_notification(lumi_shell_t *shell,
                                  const lumi_shell_notification_t *notification) {
    shell_notification_slot_t *slot;

    if (!shell || !notification || shell->notification_count >= MAX_NOTIFICATIONS) {
        return;
    }

    slot = &shell->notifications[shell->notification_count++];
    copy_string(slot->title, sizeof(slot->title), notification->title);
    copy_string(slot->body, sizeof(slot->body), notification->body);
    copy_string(slot->source, sizeof(slot->source), notification->source);
    slot->silent = notification->silent;
}

int lumi_shell_run(lumi_shell_t *shell) {
    if (!shell) {
        return -1;
    }

    shell->running = true;

    fprintf(stderr, "[shell] Display socket: %s\n",
            shell->config.wayland_display ? shell->config.wayland_display : "wayland-lumi");
    fprintf(stderr, "[shell] Guest: %s\n",
            shell->config.guest_name && shell->config.guest_name[0] != '\0'
                ? shell->config.guest_name
                : "LumiOS");
    fprintf(stderr, "[shell] Screen: %dx%d\n",
            shell->config.screen_width, shell->config.screen_height);
    fprintf(stderr, "[shell] Glass intensity: %.2f\n", shell->config.glass_intensity);
    fprintf(stderr, "[shell] Accessibility: motion=%s transparency=%s\n",
            shell->config.reduce_motion ? "reduced" : "full",
            shell->config.reduce_transparency ? "reduced" : "full");

    if (shell->config.wallpaper_path && shell->config.wallpaper_path[0] != '\0') {
        fprintf(stderr, "[shell] Wallpaper: %s\n", shell->config.wallpaper_path);
    } else {
        fprintf(stderr, "[shell] Wallpaper: default gradient\n");
    }

    if (shell->config.packages_dir && shell->config.packages_dir[0] != '\0') {
        fprintf(stderr, "[shell] Packages: %s\n", shell->config.packages_dir);
    } else {
        fprintf(stderr, "[shell] Packages: using bundled defaults\n");
    }

    if (shell->config.guest_manifest_path && shell->config.guest_manifest_path[0] != '\0') {
        fprintf(stderr, "[shell] Guest manifest: %s\n", shell->config.guest_manifest_path);
    }

    print_shared_session(shell);
    print_status_bar(shell);
    print_surface_summary(shell);
    print_app_context(shell);
    print_dock(shell);

    fprintf(stderr, "[shell] Launcher with %d apps\n", shell->app_count);
    for (int i = 0; i < shell->app_count; i++) {
        fprintf(stderr, "[shell]   [%d] %-10s (%s",
                i,
                shell->apps[i].name[0] ? shell->apps[i].name : "?",
                shell->apps[i].app_id[0] ? shell->apps[i].app_id : "?");
        if (shell->apps[i].version[0] != '\0') {
            fprintf(stderr, " v%s", shell->apps[i].version);
        }
        fprintf(stderr, ")\n");
    }

    print_notifications(shell);
    fprintf(stderr, "[shell] Community edition running\n");

    if (shell->config.keep_alive) {
        fprintf(stderr, "[shell] Session hold enabled\n");
        while (shell->running) {
            poll_runtime_state(shell);
            (void)lumi_shell_wait_for_runtime_signal(shell, 1000U);
        }
    }

    return 0;
}
