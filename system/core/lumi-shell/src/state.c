#include "shell_internal.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void copy_string(char *dst, size_t len, const char *src) {
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

static void normalize_separators(char *path) {
    if (!path) {
        return;
    }

    for (char *cursor = path; *cursor; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
}

static void sanitize_text_in_place(char *value) {
    char *read_cursor;
    char *write_cursor;

    if (!value) {
        return;
    }

    read_cursor = value;
    write_cursor = value;
    while (*read_cursor != '\0') {
        unsigned char ch = (unsigned char)*read_cursor++;

        if (ch == '\r' || ch == '\n' || ch == '\t') {
            ch = ' ';
        }
        if (ch < 32 || ch == 127) {
            continue;
        }

        *write_cursor++ = (char)ch;
    }
    *write_cursor = '\0';

    while (write_cursor > value && write_cursor[-1] == ' ') {
        *--write_cursor = '\0';
    }
}

static bool is_valid_app_id_char(char ch) {
    return isalnum((unsigned char)ch) || ch == '.' || ch == '_' || ch == '-';
}

static bool looks_like_app_id(const char *value) {
    bool has_separator = false;

    if (!value || value[0] == '\0') {
        return false;
    }
    if (!isalpha((unsigned char)value[0])) {
        return false;
    }

    for (const char *cursor = value; *cursor != '\0'; cursor++) {
        if (!is_valid_app_id_char(*cursor)) {
            return false;
        }
        if (*cursor == '.') {
            has_separator = true;
        }
    }

    return has_separator;
}

static void assign_text_if_unset(const char **target,
                                 bool overridden,
                                 char *buffer,
                                 size_t buffer_len,
                                 const char *value) {
    if (overridden || !value || value[0] == '\0') {
        return;
    }

    copy_string(buffer, buffer_len, value);
    sanitize_text_in_place(buffer);
    if (buffer[0] == '\0') {
        return;
    }
    *target = buffer;
}

static void assign_path_if_unset(const char **target,
                                 bool overridden,
                                 char *buffer,
                                 size_t buffer_len,
                                 const char *value) {
    if (overridden || !value || value[0] == '\0') {
        return;
    }

    copy_string(buffer, buffer_len, value);
    sanitize_text_in_place(buffer);
    normalize_separators(buffer);
    if (buffer[0] == '\0') {
        return;
    }
    *target = buffer;
}

static void assign_app_id_if_unset(const char **target,
                                   bool overridden,
                                   char *buffer,
                                   size_t buffer_len,
                                   const char *value) {
    if (overridden || !value || value[0] == '\0' || !looks_like_app_id(value)) {
        return;
    }

    copy_string(buffer, buffer_len, value);
    *target = buffer;
}

static int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static bool should_skip_storage_line(const char *line) {
    const char *cursor = line;

    if (!cursor) {
        return true;
    }

    while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
        cursor++;
    }

    return *cursor == '\0' || *cursor == '#';
}

static bool parse_bool_value(const char *value, bool fallback) {
    if (!value || value[0] == '\0') {
        return fallback;
    }

    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "on") == 0 ||
           strcmp(value, "yes") == 0;
}

static int parse_int_value(const char *value, int fallback) {
    char *end = NULL;
    long parsed;

    if (!value || value[0] == '\0') {
        return fallback;
    }

    parsed = strtol(value, &end, 10);
    if (!end || *end != '\0') {
        return fallback;
    }
    if (parsed < 0) {
        return 0;
    }
    if (parsed > 100) {
        return 100;
    }
    return (int)parsed;
}

static char *hex_decode(const char *value) {
    size_t len;
    char *decoded;

    if (!value) {
        return NULL;
    }

    len = strlen(value);
    if ((len % 2) != 0) {
        return NULL;
    }

    decoded = (char *)malloc(len / 2 + 1);
    if (!decoded) {
        return NULL;
    }

    for (size_t i = 0; i < len; i += 2) {
        int high = hex_value(value[i]);
        int low = hex_value(value[i + 1]);
        if (high < 0 || low < 0) {
            free(decoded);
            return NULL;
        }
        decoded[i / 2] = (char)((high << 4) | low);
    }
    decoded[len / 2] = '\0';
    return decoded;
}

static void default_storage_path(char *buffer, size_t len) {
    const char *base = NULL;

    if (!buffer || len == 0) {
        return;
    }

#ifdef _WIN32
    base = getenv("APPDATA");
    if (base && base[0] != '\0') {
        snprintf(buffer, len, "%s/LumiOS/storage.db", base);
    } else {
        base = getenv("USERPROFILE");
        snprintf(buffer, len, "%s/.lumios/storage.db", base && base[0] != '\0' ? base : ".");
    }
#else
    base = getenv("XDG_DATA_HOME");
    if (base && base[0] != '\0') {
        snprintf(buffer, len, "%s/lumios/storage.db", base);
    } else {
        base = getenv("HOME");
        snprintf(buffer, len, "%s/.lumios/storage.db", base && base[0] != '\0' ? base : ".");
    }
#endif

    normalize_separators(buffer);
}

static void resolve_storage_path(const lumi_shell_config_t *config,
                                 char *buffer,
                                 size_t len) {
    const char *storage = NULL;

    if (!buffer || len == 0) {
        return;
    }

    storage = config ? config->storage_path : NULL;
    if (!storage || storage[0] == '\0') {
        storage = getenv("LUMI_STORAGE_PATH");
    }

    if (storage && storage[0] != '\0') {
        copy_string(buffer, len, storage);
        normalize_separators(buffer);
        return;
    }

    default_storage_path(buffer, len);
}

static bool read_storage_signature(const char *path,
                                   lumi_shell_storage_signature_t *signature) {
    struct stat st;

    if (!path || !signature) {
        return false;
    }

    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            signature->known = true;
            signature->present = false;
            signature->mtime = -1;
            signature->size = -1;
            return true;
        }
        return false;
    }

    signature->known = true;
    signature->present = true;
    signature->mtime = (long long)st.st_mtime;
    signature->size = (long long)st.st_size;
    return true;
}

static bool storage_signature_equals(const lumi_shell_storage_signature_t *left,
                                     const lumi_shell_storage_signature_t *right) {
    if (!left || !right) {
        return false;
    }
    if (!left->known || !right->known) {
        return false;
    }
    if (left->present != right->present) {
        return false;
    }
    if (!left->present) {
        return true;
    }
    return left->mtime == right->mtime && left->size == right->size;
}

static void reset_cached_storage_state(lumi_shell_shared_state_t *shared_state) {
    if (!shared_state) {
        return;
    }

    shared_state->active_app[0] = '\0';
    shared_state->workspace_path[0] = '\0';
    shared_state->status_line[0] = '\0';
    shared_state->recent_action[0] = '\0';
}

static void apply_entry(lumi_shell_config_t *config,
                        const lumi_shell_override_t *overrides,
                        lumi_shell_shared_state_t *shared_state,
                        lumi_shell_notification_t *derived_notification,
                        bool *has_notification,
                        const char *key,
                        const char *value) {
    if (!config || !overrides || !shared_state || !key || !value) {
        return;
    }

    if (strcmp(key, "system.active_app") == 0) {
        assign_app_id_if_unset(&config->active_app,
                               overrides->active_app,
                               shared_state->active_app,
                               sizeof(shared_state->active_app),
                               value);
        return;
    }
    if (strcmp(key, "system.workspace_path") == 0) {
        assign_path_if_unset(&config->workspace_path,
                             overrides->workspace_path,
                             shared_state->workspace_path,
                             sizeof(shared_state->workspace_path),
                             value);
        return;
    }
    if (strcmp(key, "system.status_line") == 0) {
        assign_text_if_unset(&config->status_line,
                             overrides->status_line,
                             shared_state->status_line,
                             sizeof(shared_state->status_line),
                             value);
        return;
    }
    if (strcmp(key, "system.recent_action") == 0) {
        assign_text_if_unset(&config->recent_action,
                             overrides->recent_action,
                             shared_state->recent_action,
                             sizeof(shared_state->recent_action),
                             value);
        if (has_notification &&
            derived_notification &&
            !*has_notification &&
            shared_state->recent_action[0] != '\0') {
            derived_notification->title = "System";
            derived_notification->body = shared_state->recent_action;
            derived_notification->source = config->status_line ? config->status_line : "Session";
            derived_notification->silent = false;
            *has_notification = true;
        }
        return;
    }
    if (strcmp(key, "system.wifi") == 0 && !overrides->wifi_enabled) {
        config->wifi_enabled = parse_bool_value(value, config->wifi_enabled);
        return;
    }
    if (strcmp(key, "system.bluetooth") == 0 && !overrides->bluetooth_enabled) {
        config->bluetooth_enabled = parse_bool_value(value, config->bluetooth_enabled);
        return;
    }
    if (strcmp(key, "system.mobile_data") == 0 && !overrides->mobile_data_enabled) {
        config->mobile_data_enabled = parse_bool_value(value, config->mobile_data_enabled);
        return;
    }
    if (strcmp(key, "system.dnd") == 0 && !overrides->do_not_disturb) {
        config->do_not_disturb = parse_bool_value(value, config->do_not_disturb);
        return;
    }
    if (strcmp(key, "system.dark_mode") == 0 && !overrides->dark_mode) {
        config->dark_mode = parse_bool_value(value, config->dark_mode);
        return;
    }
    if (strcmp(key, "system.night_light") == 0 && !overrides->night_light) {
        config->night_light = parse_bool_value(value, config->night_light);
        return;
    }
    if (strcmp(key, "system.battery_saver") == 0 && !overrides->battery_saver) {
        config->battery_saver = parse_bool_value(value, config->battery_saver);
        return;
    }
    if (strcmp(key, "system.auto_brightness") == 0 && !overrides->auto_brightness) {
        config->auto_brightness = parse_bool_value(value, config->auto_brightness);
        return;
    }
    if (strcmp(key, "system.battery_level") == 0 && !overrides->battery_level) {
        config->battery_level = parse_int_value(value, config->battery_level);
        return;
    }
    if (strcmp(key, "system.brightness") == 0 && !overrides->brightness_level) {
        config->brightness_level = parse_int_value(value, config->brightness_level);
        return;
    }
    if (strcmp(key, "system.volume") == 0 && !overrides->volume_level) {
        config->volume_level = parse_int_value(value, config->volume_level);
        return;
    }
}

void lumi_shell_apply_shared_state(lumi_shell_config_t *config,
                                   const lumi_shell_override_t *overrides,
                                   lumi_shell_shared_state_t *shared_state,
                                   lumi_shell_notification_t *derived_notification,
                                   bool *has_notification) {
    char line[4096];
    lumi_shell_storage_signature_t current_signature = {0};
    FILE *file;

    if (!config || !overrides || !shared_state) {
        return;
    }

    reset_cached_storage_state(shared_state);
    if (has_notification) {
        *has_notification = false;
    }
    if (derived_notification) {
        memset(derived_notification, 0, sizeof(*derived_notification));
    }

    resolve_storage_path(config, shared_state->storage_path, sizeof(shared_state->storage_path));
    if (read_storage_signature(shared_state->storage_path, &current_signature)) {
        shared_state->storage_signature = current_signature;
    }
    config->storage_path = shared_state->storage_path;

    file = fopen(shared_state->storage_path, "rb");
    if (!file) {
        return;
    }

    while (fgets(line, sizeof(line), file)) {
        char *separator;
        char *decoded_key;
        char *decoded_value;

        line[strcspn(line, "\r\n")] = '\0';
        if (should_skip_storage_line(line)) {
            continue;
        }
        separator = strchr(line, '\t');
        if (!separator) {
            continue;
        }
        *separator++ = '\0';

        decoded_key = hex_decode(line);
        decoded_value = hex_decode(separator);
        if (!decoded_key || !decoded_value) {
            free(decoded_key);
            free(decoded_value);
            continue;
        }

        apply_entry(config,
                    overrides,
                    shared_state,
                    derived_notification,
                    has_notification,
                    decoded_key,
                    decoded_value);

        free(decoded_key);
        free(decoded_value);
    }

    fclose(file);
}

bool lumi_shell_refresh_shared_state(lumi_shell_config_t *config,
                                     const lumi_shell_override_t *overrides,
                                     lumi_shell_shared_state_t *shared_state,
                                     lumi_shell_notification_t *derived_notification,
                                     bool *has_notification) {
    char resolved_path[MAX_PATH_LEN];
    lumi_shell_storage_signature_t current_signature = {0};

    if (!config || !overrides || !shared_state) {
        return false;
    }

    resolve_storage_path(config, resolved_path, sizeof(resolved_path));
    if (!read_storage_signature(resolved_path, &current_signature)) {
        lumi_shell_apply_shared_state(config,
                                      overrides,
                                      shared_state,
                                      derived_notification,
                                      has_notification);
        return true;
    }

    if (!shared_state->storage_signature.known ||
        strcmp(shared_state->storage_path, resolved_path) != 0 ||
        !storage_signature_equals(&shared_state->storage_signature, &current_signature)) {
        lumi_shell_apply_shared_state(config,
                                      overrides,
                                      shared_state,
                                      derived_notification,
                                      has_notification);
        return true;
    }

    config->storage_path = shared_state->storage_path;
    if (has_notification) {
        *has_notification = false;
    }
    if (derived_notification) {
        memset(derived_notification, 0, sizeof(*derived_notification));
    }
    return false;
}
