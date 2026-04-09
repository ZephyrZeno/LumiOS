#include "shell_internal.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PENDING_NOTIFICATIONS 16

static bool parse_screen_size(const char *value, int *width, int *height) {
    return value && width && height && sscanf(value, "%dx%d", width, height) == 2;
}

static bool parse_toggle_value(const char *value, bool *target) {
    if (!value || !target) {
        return false;
    }

    if (strcmp(value, "on") == 0 || strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
        *target = true;
        return true;
    }
    if (strcmp(value, "off") == 0 || strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
        *target = false;
        return true;
    }

    return false;
}

static bool is_supported_panel(const char *panel) {
    static const char *panels[] = {
        "home",
        "launcher",
        "quick-settings",
        "notifications",
        "recents",
        "lockscreen",
    };

    if (!panel || panel[0] == '\0') {
        return false;
    }

    for (size_t i = 0; i < sizeof(panels) / sizeof(panels[0]); i++) {
        if (strcmp(panel, panels[i]) == 0) {
            return true;
        }
    }
    return false;
}

static int parse_percent(const char *value) {
    char *end = NULL;
    long parsed;

    if (!value || value[0] == '\0') {
        return -1;
    }

    parsed = strtol(value, &end, 10);
    if (!end || *end != '\0') {
        return -1;
    }
    if (parsed < 0 || parsed > 100) {
        return -1;
    }
    return (int)parsed;
}

static void usage(void) {
    fprintf(stderr,
            "lumi-shell (community edition)\n"
            "Usage: lumi-shell [options]\n"
            "\n"
            "Options:\n"
            "  --help                   Show this help message\n"
            "  --list-apps              Print the launcher catalog\n"
            "  --screen <WxH>           Override screen size (default 1080x2400)\n"
            "  --wallpaper <path>       Set wallpaper path for shell summary\n"
            "  --glass <0.0-1.0>        Set liquid glass intensity\n"
            "  --guest-name <name>      Set guest name shown in shell summary\n"
            "  --packages-dir <path>    Set packaged LumiApp directory\n"
            "  --guest-manifest <path>  Load app catalog from guest-manifest.json\n"
            "  --storage-path <path>    Override shared LumiOS storage database\n"
            "  --lumid-socket <path>    Override lumid daemon socket path\n"
            "  --workspace <path>       Override shared workspace path\n"
            "  --status <text>          Override shared session status line\n"
            "  --recent-action <text>   Override recent session action summary\n"
            "  --panel <name>           Open home, launcher, quick-settings,\n"
            "                           notifications, recents, or lockscreen\n"
            "  --launch <app-id>        Set the active app surface\n"
            "  --time <HH:MM>           Override status bar time\n"
            "  --battery <0-100>        Override battery level\n"
            "  --brightness <0-100>     Override quick-settings brightness\n"
            "  --volume <0-100>         Override quick-settings volume\n"
            "  --wifi <on|off>          Toggle Wi-Fi state\n"
            "  --bluetooth <on|off>     Toggle Bluetooth state\n"
            "  --mobile-data <on|off>   Toggle mobile data state\n"
            "  --dnd <on|off>           Toggle do-not-disturb\n"
            "  --dark-mode <on|off>     Toggle dark mode state\n"
            "  --night-light <on|off>   Toggle night light state\n"
            "  --battery-saver <on|off> Toggle battery saver state\n"
            "  --auto-brightness <on|off>\n"
            "                           Toggle auto brightness state\n"
            "  --locked                 Start on lockscreen\n"
            "  --notify <title|body|source>\n"
            "                           Queue a notification for the session\n"
            "  --reduce-motion          Prefer reduced motion\n"
            "  --reduce-transparency    Prefer solid surfaces over blur\n"
            "  --stay-alive             Keep the shell process alive\n"
            "\n"
            "Environment:\n"
            "  WAYLAND_DISPLAY          Wayland socket name\n"
            "  LUMI_WALLPAPER           Wallpaper path fallback\n"
            "  LUMI_GUEST_NAME          Guest name fallback\n"
            "  LUMI_PACKAGES_DIR        Packaged LumiApp directory fallback\n"
            "  LUMI_GUEST_MANIFEST      Guest manifest fallback\n"
            "  LUMI_STORAGE_PATH        Shared storage database fallback\n"
            "  LUMI_LUMID_SOCKET_PATH   lumid daemon socket fallback\n"
            "  LUMI_WORKSPACE_PATH      Shared workspace fallback\n"
            "  LUMI_STATUS_LINE         Shared status line fallback\n"
            "  LUMI_RECENT_ACTION       Recent action fallback\n"
            "  LUMI_ACTIVE_APP          Active app fallback\n"
            "  LUMI_OPEN_PANEL          Open panel fallback\n"
            "  LUMI_TIME                Time override fallback\n");
}

static void apply_environment(lumi_shell_config_t *config, lumi_shell_override_t *overrides) {
    const char *env_value;

    if (!config || !overrides) {
        return;
    }

    env_value = getenv("WAYLAND_DISPLAY");
    if (env_value && env_value[0] != '\0') {
        config->wayland_display = env_value;
    }

    if (!overrides->wallpaper_path) {
        env_value = getenv("LUMI_WALLPAPER");
        if (env_value && env_value[0] != '\0') {
            config->wallpaper_path = env_value;
            overrides->wallpaper_path = true;
        }
    }

    if (!overrides->guest_name) {
        env_value = getenv("LUMI_GUEST_NAME");
        if (env_value && env_value[0] != '\0') {
            config->guest_name = env_value;
            overrides->guest_name = true;
        }
    }

    if (!overrides->packages_dir) {
        env_value = getenv("LUMI_PACKAGES_DIR");
        if (env_value && env_value[0] != '\0') {
            config->packages_dir = env_value;
            overrides->packages_dir = true;
        }
    }

    if (!overrides->guest_manifest_path) {
        env_value = getenv("LUMI_GUEST_MANIFEST");
        if (env_value && env_value[0] != '\0') {
            config->guest_manifest_path = env_value;
            overrides->guest_manifest_path = true;
        }
    }

    if (!overrides->storage_path) {
        env_value = getenv("LUMI_STORAGE_PATH");
        if (env_value && env_value[0] != '\0') {
            config->storage_path = env_value;
            overrides->storage_path = true;
        }
    }

    if (!overrides->lumid_socket_path) {
        env_value = getenv("LUMI_LUMID_SOCKET_PATH");
        if (env_value && env_value[0] != '\0') {
            config->lumid_socket_path = env_value;
            overrides->lumid_socket_path = true;
        }
    }

    if (!overrides->workspace_path) {
        env_value = getenv("LUMI_WORKSPACE_PATH");
        if (env_value && env_value[0] != '\0') {
            config->workspace_path = env_value;
            overrides->workspace_path = true;
        }
    }

    if (!overrides->status_line) {
        env_value = getenv("LUMI_STATUS_LINE");
        if (env_value && env_value[0] != '\0') {
            config->status_line = env_value;
            overrides->status_line = true;
        }
    }

    if (!overrides->recent_action) {
        env_value = getenv("LUMI_RECENT_ACTION");
        if (env_value && env_value[0] != '\0') {
            config->recent_action = env_value;
            overrides->recent_action = true;
        }
    }

    if (!overrides->active_app) {
        env_value = getenv("LUMI_ACTIVE_APP");
        if (env_value && env_value[0] != '\0') {
            config->active_app = env_value;
            overrides->active_app = true;
        }
    }

    if (!overrides->open_panel) {
        env_value = getenv("LUMI_OPEN_PANEL");
        if (env_value && env_value[0] != '\0') {
            config->open_panel = env_value;
            overrides->open_panel = true;
        }
    }

    if (!overrides->time_override) {
        env_value = getenv("LUMI_TIME");
        if (env_value && env_value[0] != '\0') {
            config->time_override = env_value;
            overrides->time_override = true;
        }
    }
}

static void queue_notification_from_arg(lumi_shell_t *shell, const char *raw) {
    char buffer[320];
    char *title;
    char *body;
    char *source;
    lumi_shell_notification_t notification = {
        .title = "LumiOS",
        .body = "",
        .source = "System",
        .silent = false,
    };

    if (!shell || !raw || raw[0] == '\0') {
        return;
    }

    strncpy(buffer, raw, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    title = buffer;
    body = strchr(title, '|');
    if (body) {
        *body++ = '\0';
    }
    source = body ? strchr(body, '|') : NULL;
    if (source) {
        *source++ = '\0';
    }

    if (title[0] != '\0') {
        notification.title = title;
    }
    if (body && body[0] != '\0') {
        notification.body = body;
    }
    if (source && source[0] != '\0') {
        notification.source = source;
    }

    lumi_shell_push_notification(shell, &notification);
}

int main(int argc, char **argv) {
    lumi_shell_config_t config = {
        .screen_width = 1080,
        .screen_height = 2400,
        .wayland_display = "wayland-lumi",
        .wallpaper_path = NULL,
        .guest_name = "LumiOS Dev VM",
        .packages_dir = NULL,
        .guest_manifest_path = NULL,
        .storage_path = NULL,
        .lumid_socket_path = NULL,
        .workspace_path = NULL,
        .status_line = NULL,
        .recent_action = NULL,
        .time_override = NULL,
        .active_app = NULL,
        .daemon_last_event_type = NULL,
        .daemon_last_event_source = NULL,
        .open_panel = "home",
        .reduce_motion = false,
        .reduce_transparency = false,
        .keep_alive = false,
        .locked = false,
        .daemon_connected = false,
        .wifi_enabled = true,
        .bluetooth_enabled = true,
        .mobile_data_enabled = true,
        .do_not_disturb = false,
        .dark_mode = false,
        .night_light = true,
        .battery_saver = false,
        .auto_brightness = true,
        .battery_level = 86,
        .brightness_level = 72,
        .volume_level = 28,
        .daemon_event_count = 0,
        .glass_intensity = 0.72f,
    };
    lumi_shell_override_t overrides = {0};
    lumi_shell_config_t base_config;
    lumi_shell_shared_state_t shared_state;
    lumi_shell_notification_t shared_notification = {0};
    lumi_shell_notification_t daemon_notifications[MAX_NOTIFICATIONS];
    int daemon_notification_count = 0;
    uint64_t daemon_last_seen_event_id = 0;
    const char *pending_notifications[MAX_PENDING_NOTIFICATIONS];
    int pending_notification_count = 0;
    bool list_apps = false;
    bool has_shared_notification = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        }
        if (strcmp(argv[i], "--list-apps") == 0) {
            list_apps = true;
            continue;
        }
        if (strcmp(argv[i], "--screen") == 0 && i + 1 < argc) {
            if (!parse_screen_size(argv[++i], &config.screen_width, &config.screen_height)) {
                fprintf(stderr, "Invalid screen size format. Expected WxH.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[i], "--wallpaper") == 0 && i + 1 < argc) {
            config.wallpaper_path = argv[++i];
            overrides.wallpaper_path = true;
            continue;
        }
        if (strcmp(argv[i], "--glass") == 0 && i + 1 < argc) {
            config.glass_intensity = (float)atof(argv[++i]);
            if (config.glass_intensity < 0.0f) {
                config.glass_intensity = 0.0f;
            }
            if (config.glass_intensity > 1.0f) {
                config.glass_intensity = 1.0f;
            }
            continue;
        }
        if (strcmp(argv[i], "--guest-name") == 0 && i + 1 < argc) {
            config.guest_name = argv[++i];
            overrides.guest_name = true;
            continue;
        }
        if (strcmp(argv[i], "--packages-dir") == 0 && i + 1 < argc) {
            config.packages_dir = argv[++i];
            overrides.packages_dir = true;
            continue;
        }
        if (strcmp(argv[i], "--guest-manifest") == 0 && i + 1 < argc) {
            config.guest_manifest_path = argv[++i];
            overrides.guest_manifest_path = true;
            continue;
        }
        if (strcmp(argv[i], "--storage-path") == 0 && i + 1 < argc) {
            config.storage_path = argv[++i];
            overrides.storage_path = true;
            continue;
        }
        if (strcmp(argv[i], "--lumid-socket") == 0 && i + 1 < argc) {
            config.lumid_socket_path = argv[++i];
            overrides.lumid_socket_path = true;
            continue;
        }
        if (strcmp(argv[i], "--workspace") == 0 && i + 1 < argc) {
            config.workspace_path = argv[++i];
            overrides.workspace_path = true;
            continue;
        }
        if (strcmp(argv[i], "--status") == 0 && i + 1 < argc) {
            config.status_line = argv[++i];
            overrides.status_line = true;
            continue;
        }
        if (strcmp(argv[i], "--recent-action") == 0 && i + 1 < argc) {
            config.recent_action = argv[++i];
            overrides.recent_action = true;
            continue;
        }
        if (strcmp(argv[i], "--panel") == 0 && i + 1 < argc) {
            config.open_panel = argv[++i];
            overrides.open_panel = true;
            if (!is_supported_panel(config.open_panel)) {
                fprintf(stderr, "Unsupported panel: %s\n", config.open_panel);
                return 1;
            }
            continue;
        }
        if (strcmp(argv[i], "--launch") == 0 && i + 1 < argc) {
            config.active_app = argv[++i];
            overrides.active_app = true;
            continue;
        }
        if (strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            config.time_override = argv[++i];
            overrides.time_override = true;
            continue;
        }
        if (strcmp(argv[i], "--battery") == 0 && i + 1 < argc) {
            config.battery_level = parse_percent(argv[++i]);
            if (config.battery_level < 0) {
                fprintf(stderr, "Invalid battery level. Expected 0-100.\n");
                return 1;
            }
            overrides.battery_level = true;
            continue;
        }
        if (strcmp(argv[i], "--brightness") == 0 && i + 1 < argc) {
            config.brightness_level = parse_percent(argv[++i]);
            if (config.brightness_level < 0) {
                fprintf(stderr, "Invalid brightness level. Expected 0-100.\n");
                return 1;
            }
            overrides.brightness_level = true;
            continue;
        }
        if (strcmp(argv[i], "--volume") == 0 && i + 1 < argc) {
            config.volume_level = parse_percent(argv[++i]);
            if (config.volume_level < 0) {
                fprintf(stderr, "Invalid volume level. Expected 0-100.\n");
                return 1;
            }
            overrides.volume_level = true;
            continue;
        }
        if (strcmp(argv[i], "--wifi") == 0 && i + 1 < argc) {
            if (!parse_toggle_value(argv[++i], &config.wifi_enabled)) {
                fprintf(stderr, "Invalid Wi-Fi value. Use on/off.\n");
                return 1;
            }
            overrides.wifi_enabled = true;
            continue;
        }
        if (strcmp(argv[i], "--bluetooth") == 0 && i + 1 < argc) {
            if (!parse_toggle_value(argv[++i], &config.bluetooth_enabled)) {
                fprintf(stderr, "Invalid Bluetooth value. Use on/off.\n");
                return 1;
            }
            overrides.bluetooth_enabled = true;
            continue;
        }
        if (strcmp(argv[i], "--mobile-data") == 0 && i + 1 < argc) {
            if (!parse_toggle_value(argv[++i], &config.mobile_data_enabled)) {
                fprintf(stderr, "Invalid mobile data value. Use on/off.\n");
                return 1;
            }
            overrides.mobile_data_enabled = true;
            continue;
        }
        if (strcmp(argv[i], "--dnd") == 0 && i + 1 < argc) {
            if (!parse_toggle_value(argv[++i], &config.do_not_disturb)) {
                fprintf(stderr, "Invalid DND value. Use on/off.\n");
                return 1;
            }
            overrides.do_not_disturb = true;
            continue;
        }
        if (strcmp(argv[i], "--dark-mode") == 0 && i + 1 < argc) {
            if (!parse_toggle_value(argv[++i], &config.dark_mode)) {
                fprintf(stderr, "Invalid dark mode value. Use on/off.\n");
                return 1;
            }
            overrides.dark_mode = true;
            continue;
        }
        if (strcmp(argv[i], "--night-light") == 0 && i + 1 < argc) {
            if (!parse_toggle_value(argv[++i], &config.night_light)) {
                fprintf(stderr, "Invalid night light value. Use on/off.\n");
                return 1;
            }
            overrides.night_light = true;
            continue;
        }
        if (strcmp(argv[i], "--battery-saver") == 0 && i + 1 < argc) {
            if (!parse_toggle_value(argv[++i], &config.battery_saver)) {
                fprintf(stderr, "Invalid battery saver value. Use on/off.\n");
                return 1;
            }
            overrides.battery_saver = true;
            continue;
        }
        if (strcmp(argv[i], "--auto-brightness") == 0 && i + 1 < argc) {
            if (!parse_toggle_value(argv[++i], &config.auto_brightness)) {
                fprintf(stderr, "Invalid auto brightness value. Use on/off.\n");
                return 1;
            }
            overrides.auto_brightness = true;
            continue;
        }
        if (strcmp(argv[i], "--locked") == 0) {
            config.locked = true;
            continue;
        }
        if (strcmp(argv[i], "--notify") == 0 && i + 1 < argc) {
            if (pending_notification_count >= MAX_PENDING_NOTIFICATIONS) {
                fprintf(stderr, "Too many notifications queued.\n");
                return 1;
            }
            pending_notifications[pending_notification_count++] = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--reduce-motion") == 0) {
            config.reduce_motion = true;
            continue;
        }
        if (strcmp(argv[i], "--reduce-transparency") == 0) {
            config.reduce_transparency = true;
            continue;
        }
        if (strcmp(argv[i], "--stay-alive") == 0) {
            config.keep_alive = true;
            continue;
        }

        fprintf(stderr, "Unknown option: %s\n", argv[i]);
        usage();
        return 1;
    }

    apply_environment(&config, &overrides);
    base_config = config;

    {
        lumi_shell_t *shell = lumi_shell_create(&base_config);
        if (!shell) {
            return 1;
        }

        shell->base_config = base_config;
        shell->overrides = overrides;
        shell->shared_state = &shared_state;
        shell->daemon_last_seen_event_id = daemon_last_seen_event_id;
        lumi_shell_refresh_runtime_state(shell,
                                         &shared_notification,
                                         &has_shared_notification,
                                         daemon_notifications,
                                         &daemon_notification_count);

        if (shell->config.open_panel && !is_supported_panel(shell->config.open_panel)) {
            fprintf(stderr,
                    "Unsupported panel from environment/shared state: %s\n",
                    shell->config.open_panel);
            lumi_shell_destroy(shell);
            return 1;
        }

        lumi_shell_populate_catalog(shell);

        if (pending_notification_count == 0 &&
            daemon_notification_count == 0 &&
            has_shared_notification) {
            lumi_shell_push_notification(shell, &shared_notification);
        }

        for (int i = 0; i < daemon_notification_count; i++) {
            lumi_shell_push_notification(shell, &daemon_notifications[i]);
        }

        for (int i = 0; i < pending_notification_count; i++) {
            queue_notification_from_arg(shell, pending_notifications[i]);
        }

        if (list_apps) {
            lumi_shell_print_catalog(shell);
            lumi_shell_destroy(shell);
            return 0;
        }

        {
            int ret = lumi_shell_run(shell);
            lumi_shell_destroy(shell);
            return ret;
        }
    }
}
