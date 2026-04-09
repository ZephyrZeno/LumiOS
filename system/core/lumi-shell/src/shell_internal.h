#ifndef LUMI_SHELL_INTERNAL_H
#define LUMI_SHELL_INTERNAL_H

#include <stdint.h>

#include "shell.h"

#define MAX_APPS 64
#define MAX_NOTIFICATIONS 16
#define MAX_APP_ID_LEN 128
#define MAX_NAME_LEN 64
#define MAX_ICON_LEN 128
#define MAX_VERSION_LEN 32
#define MAX_BODY_LEN 192
#define MAX_STATUS_LEN 128
#define MAX_SOURCE_LEN 64
#define MAX_PATH_LEN 512

typedef struct {
    const char *app_id;
    const char *name;
    const char *icon;
    const char *version;
} bundled_app_t;

typedef struct {
    char app_id[MAX_APP_ID_LEN];
    char name[MAX_NAME_LEN];
    char icon[MAX_ICON_LEN];
    char version[MAX_VERSION_LEN];
} shell_app_slot_t;

typedef struct {
    char title[MAX_NAME_LEN];
    char body[MAX_BODY_LEN];
    char source[MAX_SOURCE_LEN];
    bool silent;
} shell_notification_slot_t;

typedef struct {
    bool wallpaper_path;
    bool guest_name;
    bool packages_dir;
    bool guest_manifest_path;
    bool storage_path;
    bool lumid_socket_path;
    bool workspace_path;
    bool status_line;
    bool recent_action;
    bool time_override;
    bool active_app;
    bool open_panel;
    bool wifi_enabled;
    bool bluetooth_enabled;
    bool mobile_data_enabled;
    bool do_not_disturb;
    bool dark_mode;
    bool night_light;
    bool battery_saver;
    bool auto_brightness;
    bool battery_level;
    bool brightness_level;
    bool volume_level;
} lumi_shell_override_t;

typedef struct {
    bool known;
    bool present;
    long long mtime;
    long long size;
} lumi_shell_storage_signature_t;

typedef struct {
    char storage_path[MAX_PATH_LEN];
    char lumid_socket_path[MAX_PATH_LEN];
    char active_app[MAX_APP_ID_LEN];
    char workspace_path[MAX_PATH_LEN];
    char status_line[MAX_STATUS_LEN];
    char recent_action[MAX_BODY_LEN];
    lumi_shell_storage_signature_t storage_signature;
    char daemon_last_event_type[MAX_NAME_LEN];
    char daemon_last_event_source[MAX_SOURCE_LEN];
    char daemon_notification_titles[MAX_NOTIFICATIONS][MAX_NAME_LEN];
    char daemon_notification_bodies[MAX_NOTIFICATIONS][MAX_BODY_LEN];
    char daemon_notification_sources[MAX_NOTIFICATIONS][MAX_SOURCE_LEN];
    uint64_t daemon_last_event_id;
    int daemon_event_count;
    bool daemon_connected;
} lumi_shell_shared_state_t;

struct lumi_shell {
    lumi_shell_config_t config;
    lumi_shell_config_t base_config;
    lumi_shell_override_t overrides;
    lumi_shell_shared_state_t *shared_state;
    shell_app_slot_t apps[MAX_APPS];
    shell_notification_slot_t notifications[MAX_NOTIFICATIONS];
    uint64_t daemon_last_seen_event_id;
    int app_count;
    int notification_count;
    bool running;
};

void lumi_shell_apply_shared_state(lumi_shell_config_t *config,
                                   const lumi_shell_override_t *overrides,
                                   lumi_shell_shared_state_t *shared_state,
                                   lumi_shell_notification_t *derived_notification,
                                   bool *has_notification);
bool lumi_shell_refresh_shared_state(lumi_shell_config_t *config,
                                     const lumi_shell_override_t *overrides,
                                     lumi_shell_shared_state_t *shared_state,
                                     lumi_shell_notification_t *derived_notification,
                                     bool *has_notification);
void lumi_shell_apply_daemon_events(lumi_shell_config_t *config,
                                    const lumi_shell_override_t *overrides,
                                    lumi_shell_shared_state_t *shared_state,
                                    lumi_shell_notification_t *notifications,
                                    int *notification_count,
                                    int max_notifications,
                                    uint64_t *last_seen_event_id);
void lumi_shell_refresh_runtime_state(lumi_shell_t *shell,
                                      lumi_shell_notification_t *shared_notification,
                                      bool *has_shared_notification,
                                      lumi_shell_notification_t *daemon_notifications,
                                      int *daemon_notification_count);
bool lumi_shell_wait_for_runtime_signal(const lumi_shell_t *shell, unsigned int timeout_ms);

#endif /* LUMI_SHELL_INTERNAL_H */
