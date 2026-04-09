#include "shell_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const bundled_app_t k_default_apps[] = {
    { "com.lumios.browser",  "Browser",  NULL, "1.1.0" },
    { "com.lumios.camera",   "Camera",   NULL, "1.1.0" },
    { "com.lumios.files",    "Files",    NULL, "1.1.0" },
    { "com.lumios.messages", "Messages", NULL, "1.1.0" },
    { "com.lumios.phone",    "Phone",    NULL, "1.1.0" },
    { "com.lumios.settings", "Settings", NULL, "1.1.0" },
    { "com.lumios.terminal", "Terminal", NULL, "1.1.0" },
};

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

static const char *lookup_default_name(const char *app_id) {
    size_t count = sizeof(k_default_apps) / sizeof(k_default_apps[0]);

    for (size_t i = 0; i < count; i++) {
        if (app_id && strcmp(app_id, k_default_apps[i].app_id) == 0) {
            return k_default_apps[i].name;
        }
    }
    return NULL;
}

static void infer_name_from_app_id(const char *app_id, char *output, size_t output_len) {
    const char *known = lookup_default_name(app_id);

    if (known) {
        copy_string(output, output_len, known);
        return;
    }

    if (!app_id || app_id[0] == '\0') {
        copy_string(output, output_len, "App");
        return;
    }

    {
        const char *cursor = strrchr(app_id, '.');
        bool capitalize = true;
        char buffer[MAX_NAME_LEN];
        size_t used = 0;

        if (cursor && cursor[1] != '\0') {
            cursor++;
        } else {
            cursor = app_id;
        }

        while (*cursor && used + 1 < sizeof(buffer)) {
            char ch = *cursor++;
            if (ch == '-' || ch == '_' || ch == '.') {
                if (used > 0 && buffer[used - 1] != ' ' && used + 1 < sizeof(buffer)) {
                    buffer[used++] = ' ';
                }
                capitalize = true;
                continue;
            }
            if (capitalize && ch >= 'a' && ch <= 'z') {
                ch = (char)(ch - 'a' + 'A');
            }
            capitalize = false;
            buffer[used++] = ch;
        }

        buffer[used] = '\0';
        copy_string(output, output_len, used > 0 ? buffer : "App");
    }
}

static bool parse_json_string_value(const char *cursor,
                                    const char *key,
                                    char *output,
                                    size_t output_len) {
    const char *found;
    const char *start;
    const char *end;
    size_t len;

    if (!cursor || !key || !output || output_len == 0) {
        return false;
    }

    found = strstr(cursor, key);
    if (!found) {
        output[0] = '\0';
        return false;
    }

    found = strchr(found, ':');
    if (!found) {
        output[0] = '\0';
        return false;
    }

    start = strchr(found, '"');
    if (!start) {
        output[0] = '\0';
        return false;
    }
    start++;

    end = strchr(start, '"');
    if (!end) {
        output[0] = '\0';
        return false;
    }

    len = (size_t)(end - start);
    if (len >= output_len) {
        len = output_len - 1;
    }
    memcpy(output, start, len);
    output[len] = '\0';
    return true;
}

static char *read_text_file(const char *path) {
    FILE *file;
    long size;
    char *buffer;

    if (!path || path[0] == '\0') {
        return NULL;
    }

    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)calloc((size_t)size + 1, 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (size > 0 && fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        fclose(file);
        free(buffer);
        return NULL;
    }

    fclose(file);
    buffer[size] = '\0';
    return buffer;
}

static int load_manifest_apps(lumi_shell_t *shell, const char *manifest_path) {
    char *source;
    const char *cursor;
    int added = 0;

    if (!shell || !manifest_path || manifest_path[0] == '\0') {
        return 0;
    }

    source = read_text_file(manifest_path);
    if (!source) {
        return 0;
    }

    cursor = source;
    while ((cursor = strstr(cursor, "\"appId\"")) != NULL && shell->app_count < MAX_APPS) {
        char app_id[MAX_APP_ID_LEN];
        char name[MAX_NAME_LEN];
        char version[MAX_VERSION_LEN];
        lumi_shell_app_entry_t entry;

        if (!parse_json_string_value(cursor, "\"appId\"", app_id, sizeof(app_id))) {
            cursor += 7;
            continue;
        }

        if (!parse_json_string_value(cursor, "\"name\"", name, sizeof(name)) || name[0] == '\0') {
            infer_name_from_app_id(app_id, name, sizeof(name));
        }

        if (!parse_json_string_value(cursor, "\"version\"", version, sizeof(version))) {
            copy_string(version, sizeof(version), "unknown");
        }

        entry.app_id = app_id;
        entry.name = name;
        entry.icon = NULL;
        entry.version = version;
        lumi_shell_add_app(shell, &entry);
        added++;
        cursor += 7;
    }

    free(source);
    return added;
}

static void add_default_apps(lumi_shell_t *shell) {
    size_t count = sizeof(k_default_apps) / sizeof(k_default_apps[0]);

    for (size_t i = 0; i < count; i++) {
        lumi_shell_add_app(shell, &(lumi_shell_app_entry_t) {
            .app_id = k_default_apps[i].app_id,
            .name = k_default_apps[i].name,
            .icon = k_default_apps[i].icon,
            .version = k_default_apps[i].version,
        });
    }
}

void lumi_shell_add_app(lumi_shell_t *shell, const lumi_shell_app_entry_t *entry) {
    shell_app_slot_t *slot;

    if (!shell || !entry || shell->app_count >= MAX_APPS || !entry->app_id) {
        return;
    }

    for (int i = 0; i < shell->app_count; i++) {
        if (shell->apps[i].app_id[0] != '\0' &&
            strcmp(shell->apps[i].app_id, entry->app_id) == 0) {
            if (entry->name && entry->name[0] != '\0') {
                copy_string(shell->apps[i].name, sizeof(shell->apps[i].name), entry->name);
            }
            if (entry->icon && entry->icon[0] != '\0') {
                copy_string(shell->apps[i].icon, sizeof(shell->apps[i].icon), entry->icon);
            }
            if (entry->version && entry->version[0] != '\0') {
                copy_string(shell->apps[i].version, sizeof(shell->apps[i].version), entry->version);
            }
            return;
        }
    }

    slot = &shell->apps[shell->app_count++];
    copy_string(slot->app_id, sizeof(slot->app_id), entry->app_id);
    copy_string(slot->name, sizeof(slot->name),
                entry->name && entry->name[0] != '\0' ? entry->name : entry->app_id);
    copy_string(slot->icon, sizeof(slot->icon), entry->icon);
    copy_string(slot->version, sizeof(slot->version), entry->version);
}

void lumi_shell_populate_catalog(lumi_shell_t *shell) {
    if (!shell) {
        return;
    }

    if (load_manifest_apps(shell, shell->config.guest_manifest_path) == 0) {
        add_default_apps(shell);
    }
}

void lumi_shell_print_catalog(lumi_shell_t *shell) {
    if (!shell) {
        return;
    }

    fprintf(stderr, "Launcher apps (%d):\n", shell->app_count);
    for (int i = 0; i < shell->app_count; i++) {
        fprintf(stderr, "  %-12s %s",
                shell->apps[i].name[0] ? shell->apps[i].name : "?",
                shell->apps[i].app_id[0] ? shell->apps[i].app_id : "?");
        if (shell->apps[i].version[0] != '\0') {
            fprintf(stderr, "  v%s", shell->apps[i].version);
        }
        fputc('\n', stderr);
    }
}
