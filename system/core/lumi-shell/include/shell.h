#ifndef LUMI_SHELL_H
#define LUMI_SHELL_H

#include <stdbool.h>

typedef struct lumi_shell lumi_shell_t;

typedef struct {
    int screen_width;
    int screen_height;
    const char *wayland_display;
    const char *wallpaper_path;
    const char *guest_name;
    const char *packages_dir;
    const char *guest_manifest_path;
    const char *storage_path;
    const char *lumid_socket_path;
    const char *workspace_path;
    const char *status_line;
    const char *recent_action;
    const char *time_override;
    const char *active_app;
    const char *daemon_last_event_type;
    const char *daemon_last_event_source;
    const char *open_panel;
    bool reduce_motion;
    bool reduce_transparency;
    bool keep_alive;
    bool locked;
    bool daemon_connected;
    bool wifi_enabled;
    bool bluetooth_enabled;
    bool mobile_data_enabled;
    bool do_not_disturb;
    bool dark_mode;
    bool night_light;
    bool battery_saver;
    bool auto_brightness;
    int battery_level;
    int brightness_level;
    int volume_level;
    int daemon_event_count;
    float glass_intensity;
} lumi_shell_config_t;

typedef struct {
    const char *app_id;
    const char *name;
    const char *icon;
    const char *version;
} lumi_shell_app_entry_t;

typedef struct {
    const char *title;
    const char *body;
    const char *source;
    bool silent;
} lumi_shell_notification_t;

lumi_shell_t *lumi_shell_create(const lumi_shell_config_t *config);
void lumi_shell_destroy(lumi_shell_t *shell);
int lumi_shell_run(lumi_shell_t *shell);

void lumi_shell_add_app(lumi_shell_t *shell, const lumi_shell_app_entry_t *entry);
void lumi_shell_populate_catalog(lumi_shell_t *shell);
void lumi_shell_print_catalog(lumi_shell_t *shell);
void lumi_shell_push_notification(lumi_shell_t *shell,
                                  const lumi_shell_notification_t *notification);

#endif /* LUMI_SHELL_H */
