#include "shell_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
}

static void expect(bool condition, const char *message)
{
    if (!condition) {
        fail(message);
    }
}

static void copy_string(char *dst, size_t len, const char *src)
{
    size_t used;

    if (!dst || len == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    used = strlen(src);
    if (used >= len) {
        used = len - 1;
    }
    memcpy(dst, src, used);
    dst[used] = '\0';
}

static void hex_encode(const char *value, char *buffer, size_t len)
{
    static const char k_hex[] = "0123456789ABCDEF";
    size_t used = 0;

    if (!buffer || len == 0) {
        return;
    }

    if (!value) {
        buffer[0] = '\0';
        return;
    }

    for (const unsigned char *cursor = (const unsigned char *)value;
         *cursor != '\0' && used + 2 < len;
         cursor++) {
        buffer[used++] = k_hex[(*cursor >> 4) & 0x0F];
        buffer[used++] = k_hex[*cursor & 0x0F];
    }
    buffer[used] = '\0';
}

static void write_entry(FILE *file, const char *key, const char *value)
{
    char encoded_key[512];
    char encoded_value[1024];

    hex_encode(key, encoded_key, sizeof(encoded_key));
    hex_encode(value, encoded_value, sizeof(encoded_value));
    fprintf(file, "%s\t%s\n", encoded_key, encoded_value);
}

static void write_storage_fixture(const char *path,
                                  const char *active_app,
                                  const char *workspace,
                                  const char *status,
                                  const char *recent_action,
                                  bool wifi_enabled,
                                  int brightness,
                                  int volume)
{
    FILE *file = fopen(path, "wb");

    expect(file != NULL, "failed to create storage fixture");
    fprintf(file, "#LUMI_STORAGE_V1\n");
    write_entry(file, "system.active_app", active_app);
    write_entry(file, "system.workspace_path", workspace);
    write_entry(file, "system.status_line", status);
    write_entry(file, "system.recent_action", recent_action);
    write_entry(file, "system.wifi", wifi_enabled ? "true" : "false");

    {
        char numeric[16];

        snprintf(numeric, sizeof(numeric), "%d", brightness);
        write_entry(file, "system.brightness", numeric);
        snprintf(numeric, sizeof(numeric), "%d", volume);
        write_entry(file, "system.volume", numeric);
    }

    fclose(file);
}

static void test_runtime_refresh_reloads_storage(void)
{
    char storage_path[512];
    lumi_shell_config_t base_config = {
        .screen_width = 1080,
        .screen_height = 2400,
        .wayland_display = "wayland-test",
        .guest_name = "Test Guest",
        .open_panel = "home",
        .storage_path = storage_path,
        .wifi_enabled = true,
        .bluetooth_enabled = true,
        .mobile_data_enabled = true,
        .night_light = true,
        .auto_brightness = true,
        .battery_level = 86,
        .brightness_level = 72,
        .volume_level = 28,
        .glass_intensity = 0.72f,
    };
    lumi_shell_override_t overrides = {0};
    lumi_shell_shared_state_t shared_state = {0};
    lumi_shell_notification_t shared_notification = {0};
    lumi_shell_notification_t daemon_notifications[MAX_NOTIFICATIONS] = {0};
    bool has_shared_notification = false;
    int daemon_notification_count = 0;
    lumi_shell_t *shell;

    copy_string(storage_path, sizeof(storage_path), "build/test-runtime-storage.db");
    write_storage_fixture(storage_path,
                          "com.lumios.settings",
                          "workspace/settings",
                          "Settings synced",
                          "Settings opened",
                          false,
                          64,
                          33);

    shell = lumi_shell_create(&base_config);
    expect(shell != NULL, "failed to create shell");
    shell->base_config = base_config;
    shell->overrides = overrides;
    shell->shared_state = &shared_state;

    lumi_shell_refresh_runtime_state(shell,
                                     &shared_notification,
                                     &has_shared_notification,
                                     daemon_notifications,
                                     &daemon_notification_count);

    expect(has_shared_notification, "first refresh should derive notification");
    expect(daemon_notification_count == 0, "storage-only refresh should not create daemon notifications");
    expect(strcmp(shell->config.active_app, "com.lumios.settings") == 0,
           "first refresh active app mismatch");
    expect(strcmp(shell->config.workspace_path, "workspace/settings") == 0,
           "first refresh workspace mismatch");
    expect(strcmp(shell->config.status_line, "Settings synced") == 0,
           "first refresh status mismatch");
    expect(strcmp(shell->config.recent_action, "Settings opened") == 0,
           "first refresh recent action mismatch");
    expect(!shell->config.wifi_enabled, "first refresh wifi mismatch");
    expect(shell->config.brightness_level == 64, "first refresh brightness mismatch");
    expect(shell->config.volume_level == 33, "first refresh volume mismatch");

    write_storage_fixture(storage_path,
                          "com.lumios.files",
                          "workspace/files",
                          "Files indexed",
                          "Files opened",
                          true,
                          51,
                          12);

    has_shared_notification = false;
    shared_notification = (lumi_shell_notification_t){0};
    lumi_shell_refresh_runtime_state(shell,
                                     &shared_notification,
                                     &has_shared_notification,
                                     daemon_notifications,
                                     &daemon_notification_count);

    expect(has_shared_notification, "second refresh should derive notification");
    expect(strcmp(shell->config.active_app, "com.lumios.files") == 0,
           "second refresh active app mismatch");
    expect(strcmp(shell->config.workspace_path, "workspace/files") == 0,
           "second refresh workspace mismatch");
    expect(strcmp(shell->config.status_line, "Files indexed") == 0,
           "second refresh status mismatch");
    expect(strcmp(shell->config.recent_action, "Files opened") == 0,
           "second refresh recent action mismatch");
    expect(shell->config.wifi_enabled, "second refresh wifi mismatch");
    expect(shell->config.brightness_level == 51, "second refresh brightness mismatch");
    expect(shell->config.volume_level == 12, "second refresh volume mismatch");
    expect(strcmp(shared_notification.body, "Files opened") == 0,
           "second refresh notification mismatch");

    remove(storage_path);
    lumi_shell_destroy(shell);
}

static void test_shared_refresh_detects_noop_signature(void)
{
    char storage_path[512];
    lumi_shell_config_t config = {
        .screen_width = 1080,
        .screen_height = 2400,
        .storage_path = storage_path,
        .wifi_enabled = true,
        .bluetooth_enabled = true,
        .mobile_data_enabled = true,
        .night_light = true,
        .auto_brightness = true,
        .battery_level = 86,
        .brightness_level = 72,
        .volume_level = 28,
        .glass_intensity = 0.72f,
    };
    lumi_shell_override_t overrides = {0};
    lumi_shell_shared_state_t shared_state = {0};
    lumi_shell_notification_t shared_notification = {0};
    bool has_shared_notification = false;
    bool changed = false;

    copy_string(storage_path, sizeof(storage_path), "build/test-runtime-signature.db");
    write_storage_fixture(storage_path,
                          "com.lumios.settings",
                          "workspace/settings",
                          "Settings synced",
                          "Settings opened",
                          false,
                          64,
                          33);

    changed = lumi_shell_refresh_shared_state(&config,
                                              &overrides,
                                              &shared_state,
                                              &shared_notification,
                                              &has_shared_notification);
    expect(changed, "first signature refresh should load storage");
    expect(shared_state.storage_signature.known, "signature should be tracked after first load");

    shared_notification = (lumi_shell_notification_t){0};
    has_shared_notification = true;
    changed = lumi_shell_refresh_shared_state(&config,
                                              &overrides,
                                              &shared_state,
                                              &shared_notification,
                                              &has_shared_notification);
    expect(!changed, "unchanged signature should skip reload");
    expect(!has_shared_notification, "noop signature should not derive notification");
    expect(shared_notification.title == NULL, "noop signature should keep notification empty");

    remove(storage_path);
}

int main(void)
{
    test_runtime_refresh_reloads_storage();
    test_shared_refresh_detects_noop_signature();
    printf("runtime refresh tests passed\n");
    return 0;
}
