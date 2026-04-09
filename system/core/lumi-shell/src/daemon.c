#include "shell_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#define LUMI_LUMID_SOCKET_DEFAULT "/run/lumid.sock"
#define LUMI_LUMID_CMD_EVENT_LIST 10

#define LUMI_LUMID_MAX_NAME_LEN 64
#define LUMI_LUMID_MAX_MESSAGE_LEN 256
#define LUMI_LUMID_MAX_TYPE_LEN 64
#define LUMI_LUMID_MAX_PAYLOAD_LEN 256

typedef struct {
    int cmd;
    char service_name[LUMI_LUMID_MAX_NAME_LEN];
    char channel[LUMI_LUMID_MAX_NAME_LEN];
    char event_type[LUMI_LUMID_MAX_TYPE_LEN];
    char source[LUMI_LUMID_MAX_NAME_LEN];
    char payload[LUMI_LUMID_MAX_PAYLOAD_LEN];
    int flags;
} lumi_lumid_request_t;

typedef struct {
    int code;
    char message[LUMI_LUMID_MAX_MESSAGE_LEN];
    char channel[LUMI_LUMID_MAX_NAME_LEN];
    char event_type[LUMI_LUMID_MAX_TYPE_LEN];
    char source[LUMI_LUMID_MAX_NAME_LEN];
    char payload[LUMI_LUMID_MAX_PAYLOAD_LEN];
    int state;
    int pid;
    int exit_code;
    unsigned long long uptime;
    unsigned long long event_id;
} lumi_lumid_response_t;

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

#if !defined(_WIN32)
static void sanitize_text_in_place(char *value)
{
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

static bool is_valid_app_id_char(char ch)
{
    return isalnum((unsigned char)ch) || ch == '.' || ch == '_' || ch == '-';
}

static bool looks_like_app_id(const char *value)
{
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
#endif

static bool has_text(const char *value)
{
    return value && value[0] != '\0';
}

#if !defined(_WIN32)
static void parse_payload_field(const char *payload,
                                const char *key,
                                char *buffer,
                                size_t buffer_len)
{
    size_t key_len;
    const char *cursor;

    if (!buffer || buffer_len == 0) {
        return;
    }
    buffer[0] = '\0';
    if (!payload || !key || key[0] == '\0') {
        return;
    }

    key_len = strlen(key);
    cursor = payload;
    while (*cursor != '\0') {
        const char *separator = strchr(cursor, ';');
        size_t pair_len = separator ? (size_t)(separator - cursor) : strlen(cursor);

        if (pair_len > key_len + 1 &&
            strncmp(cursor, key, key_len) == 0 &&
            cursor[key_len] == '=') {
            size_t value_len = pair_len - key_len - 1;

            if (value_len >= buffer_len) {
                value_len = buffer_len - 1;
            }
            memcpy(buffer, cursor + key_len + 1, value_len);
            buffer[value_len] = '\0';
            sanitize_text_in_place(buffer);
            return;
        }

        if (!separator) {
            break;
        }
        cursor = separator + 1;
    }
}

static void fill_notification_from_event(lumi_shell_shared_state_t *shared_state,
                                         int notification_index,
                                         lumi_shell_notification_t *notification,
                                         const lumi_lumid_response_t *event)
{
    static const char *updated_title = "System Update";
    static const char *rejected_title = "System Change Rejected";
    static const char *generic_title = "System Event";
    char status[MAX_STATUS_LEN];
    const char *title = generic_title;
    const char *body = NULL;
    const char *source = NULL;

    if (!shared_state || notification_index < 0 || notification_index >= MAX_NOTIFICATIONS ||
        !notification || !event) {
        return;
    }

    parse_payload_field(event->payload, "status", status, sizeof(status));
    if (strcmp(event->event_type, "system.state.updated") == 0) {
        title = updated_title;
    } else if (strcmp(event->event_type, "system.state.rejected") == 0) {
        title = rejected_title;
    }

    body = has_text(status) ? status : event->payload;
    source = has_text(event->source) ? event->source : "lumid";

    copy_string(shared_state->daemon_notification_titles[notification_index],
                sizeof(shared_state->daemon_notification_titles[notification_index]),
                title);
    copy_string(shared_state->daemon_notification_bodies[notification_index],
                sizeof(shared_state->daemon_notification_bodies[notification_index]),
                body);
    copy_string(shared_state->daemon_notification_sources[notification_index],
                sizeof(shared_state->daemon_notification_sources[notification_index]),
                source);

    notification->title = shared_state->daemon_notification_titles[notification_index];
    notification->body = shared_state->daemon_notification_bodies[notification_index];
    notification->source = shared_state->daemon_notification_sources[notification_index];
    notification->silent = false;
}

static void apply_latest_event_to_config(lumi_shell_config_t *config,
                                         const lumi_shell_override_t *overrides,
                                         lumi_shell_shared_state_t *shared_state,
                                         const lumi_lumid_response_t *event)
{
    char active_app[MAX_APP_ID_LEN];
    char status[MAX_STATUS_LEN];

    if (!config || !overrides || !shared_state || !event) {
        return;
    }

    parse_payload_field(event->payload, "active_app", active_app, sizeof(active_app));
    if (!overrides->active_app && looks_like_app_id(active_app)) {
        copy_string(shared_state->active_app, sizeof(shared_state->active_app), active_app);
        config->active_app = shared_state->active_app;
    }

    parse_payload_field(event->payload, "status", status, sizeof(status));
    if (!overrides->status_line && has_text(status)) {
        copy_string(shared_state->status_line, sizeof(shared_state->status_line), status);
        config->status_line = shared_state->status_line;
    }

    if (!overrides->recent_action) {
        if (strcmp(event->event_type, "system.state.rejected") == 0) {
            snprintf(shared_state->recent_action,
                     sizeof(shared_state->recent_action),
                     "%s attempted a restricted system change",
                     has_text(event->source) ? event->source : "An app");
        } else {
            snprintf(shared_state->recent_action,
                     sizeof(shared_state->recent_action),
                     "%s synced system state",
                     has_text(event->source) ? event->source : "System");
        }
        config->recent_action = shared_state->recent_action;
    }
}

static int read_full(int fd, void *buffer, size_t size)
{
    char *cursor = buffer;
    size_t used = 0;

    while (used < size) {
        ssize_t n = read(fd, cursor + used, size - used);

        if (n == 0) {
            return -1;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        used += (size_t)n;
    }

    return 0;
}

static int write_full(int fd, const void *buffer, size_t size)
{
    const char *cursor = buffer;
    size_t used = 0;

    while (used < size) {
        ssize_t n = write(fd, cursor + used, size - used);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        used += (size_t)n;
    }

    return 0;
}
#endif

void lumi_shell_apply_daemon_events(lumi_shell_config_t *config,
                                    const lumi_shell_override_t *overrides,
                                    lumi_shell_shared_state_t *shared_state,
                                    lumi_shell_notification_t *notifications,
                                    int *notification_count,
                                    int max_notifications,
                                    uint64_t *last_seen_event_id)
{
    const char *socket_path = NULL;
    bool explicit_socket = false;

    if (!config || !overrides || !shared_state || !notification_count || max_notifications <= 0) {
        return;
    }

    *notification_count = 0;
    shared_state->daemon_connected = false;
    shared_state->daemon_event_count = 0;
    shared_state->daemon_last_event_type[0] = '\0';
    shared_state->daemon_last_event_source[0] = '\0';
    shared_state->daemon_last_event_id = 0;
    config->daemon_connected = false;
    config->daemon_event_count = 0;
    config->daemon_last_event_type = NULL;
    config->daemon_last_event_source = NULL;

    socket_path = config->lumid_socket_path;
    explicit_socket = has_text(socket_path);
    if (!has_text(socket_path)) {
        socket_path = getenv("LUMI_LUMID_SOCKET_PATH");
        explicit_socket = has_text(socket_path);
    }
    if (!has_text(socket_path)) {
#if defined(_WIN32)
        return;
#else
        socket_path = LUMI_LUMID_SOCKET_DEFAULT;
#endif
    }

#if defined(_WIN32)
    (void)notifications;
    (void)last_seen_event_id;
    if (explicit_socket) {
        copy_string(shared_state->lumid_socket_path,
                    sizeof(shared_state->lumid_socket_path),
                    socket_path);
        config->lumid_socket_path = shared_state->lumid_socket_path;
    }
    return;
#else
    {
        uint64_t seen_event_id = last_seen_event_id ? *last_seen_event_id : 0;
        int fd;
        struct sockaddr_un addr;
        lumi_lumid_request_t req;
        lumi_lumid_response_t resp;
        int buffered_count;

        fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            return;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        copy_string(addr.sun_path, sizeof(addr.sun_path), socket_path);

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            close(fd);
            if (explicit_socket) {
                copy_string(shared_state->lumid_socket_path,
                            sizeof(shared_state->lumid_socket_path),
                            socket_path);
                config->lumid_socket_path = shared_state->lumid_socket_path;
            }
            return;
        }

        copy_string(shared_state->lumid_socket_path,
                    sizeof(shared_state->lumid_socket_path),
                    socket_path);
        config->lumid_socket_path = shared_state->lumid_socket_path;

        memset(&req, 0, sizeof(req));
        req.cmd = LUMI_LUMID_CMD_EVENT_LIST;

        if (write_full(fd, &req, sizeof(req)) != 0 ||
            read_full(fd, &resp, sizeof(resp)) != 0) {
            close(fd);
            return;
        }

        if (resp.code < 0) {
            close(fd);
            return;
        }

        shared_state->daemon_connected = true;
        shared_state->daemon_event_count = resp.code;
        config->daemon_connected = true;
        config->daemon_event_count = resp.code;

        buffered_count = resp.code;

        for (int i = 0; i < buffered_count; i++) {
            lumi_lumid_response_t event_resp;

            if (read_full(fd, &event_resp, sizeof(event_resp)) != 0) {
                break;
            }

            copy_string(shared_state->daemon_last_event_type,
                        sizeof(shared_state->daemon_last_event_type),
                        event_resp.event_type);
            copy_string(shared_state->daemon_last_event_source,
                        sizeof(shared_state->daemon_last_event_source),
                        event_resp.source);
            shared_state->daemon_last_event_id = (uint64_t)event_resp.event_id;
            config->daemon_last_event_type = shared_state->daemon_last_event_type;
            config->daemon_last_event_source = shared_state->daemon_last_event_source;

            if (strcmp(event_resp.channel, "system") != 0) {
                continue;
            }
            if (strcmp(event_resp.event_type, "system.state.updated") != 0 &&
                strcmp(event_resp.event_type, "system.state.rejected") != 0) {
                continue;
            }

            if ((uint64_t)event_resp.event_id <= seen_event_id) {
                continue;
            }

            apply_latest_event_to_config(config, overrides, shared_state, &event_resp);
            if ((uint64_t)event_resp.event_id > seen_event_id) {
                seen_event_id = (uint64_t)event_resp.event_id;
            }

            if (*notification_count < max_notifications) {
                fill_notification_from_event(shared_state,
                                             *notification_count,
                                             &notifications[*notification_count],
                                             &event_resp);
                (*notification_count)++;
            }
        }

        close(fd);
        if (last_seen_event_id) {
            *last_seen_event_id = seen_event_id;
        }
    }
#endif
}
