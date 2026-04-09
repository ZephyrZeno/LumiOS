/*
 * socket.c - IPC communication via Unix domain sockets
 *
 * Provides server/client API for lumictl <-> lumid communication.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "lumid.h"

static int read_full(int fd, void *buffer, size_t size)
{
    char *cursor = buffer;
    size_t total = 0;

    while (total < size) {
        ssize_t n = read(fd, cursor + total, size - total);

        if (n == 0) {
            return -1;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += (size_t)n;
    }

    return 0;
}

static int write_full(int fd, const void *buffer, size_t size)
{
    const char *cursor = buffer;
    size_t total = 0;

    while (total < size) {
        ssize_t n = write(fd, cursor + total, size - total);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += (size_t)n;
    }

    return 0;
}

#ifndef LUMICTL_BUILD
typedef struct {
    char channel[LUMID_MAX_NAME_LEN];
    char event_type[LUMID_MAX_IPC_TYPE_LEN];
    char source[LUMID_MAX_NAME_LEN];
    char payload[LUMID_MAX_IPC_PAYLOAD];
    uint64_t timestamp_ns;
    uint64_t event_id;
} lumid_event_t;

static lumid_event_t g_events[LUMID_MAX_EVENTS];
static int g_event_count = 0;
static int g_event_next = 0;
static uint64_t g_event_next_id = 1;

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

static uint64_t record_event(const ipc_request_t *req)
{
    lumid_event_t *event;

    if (!req) {
        return 0;
    }

    event = &g_events[g_event_next];
    copy_string(event->channel, sizeof(event->channel), req->channel);
    copy_string(event->event_type, sizeof(event->event_type), req->event_type);
    copy_string(event->source, sizeof(event->source), req->source);
    copy_string(event->payload, sizeof(event->payload), req->payload);
    event->timestamp_ns = util_monotonic_ns();
    event->event_id = g_event_next_id++;

    g_event_next = (g_event_next + 1) % LUMID_MAX_EVENTS;
    if (g_event_count < LUMID_MAX_EVENTS) {
        g_event_count++;
    }

    return event->event_id;
}

static void fill_event_response(ipc_response_t *resp, const lumid_event_t *event)
{
    if (!resp || !event) {
        return;
    }

    copy_string(resp->message, sizeof(resp->message), "event");
    copy_string(resp->channel, sizeof(resp->channel), event->channel);
    copy_string(resp->event_type, sizeof(resp->event_type), event->event_type);
    copy_string(resp->source, sizeof(resp->source), event->source);
    copy_string(resp->payload, sizeof(resp->payload), event->payload);
    resp->uptime = event->timestamp_ns;
    resp->event_id = event->event_id;
}

int socket_server_init(const char *path)
{
    int fd;
    struct sockaddr_un addr;

    unlink(path);

    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        LOG_E("socket creation failed: %s", strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_E("socket bind failed on %s: %s", path, strerror(errno));
        close(fd);
        return -1;
    }

    chmod(path, 0666);

    if (listen(fd, 8) < 0) {
        LOG_E("socket listen failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    LOG_D("IPC socket listening on %s (fd %d)", path, fd);
    return fd;
}

int socket_server_accept(int server_fd)
{
    int client_fd = accept4(server_fd, NULL, NULL, SOCK_CLOEXEC);

    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_E("accept failed: %s", strerror(errno));
        }
        return -1;
    }

    return client_fd;
}

int socket_handle_request(int client_fd)
{
    ipc_request_t req;
    ipc_response_t resp;
    service_t *svc = NULL;
    uint64_t event_id = 0;

    memset(&resp, 0, sizeof(resp));

    if (read_full(client_fd, &req, sizeof(req)) < 0) {
        LOG_W("malformed IPC request (expected %zu bytes)", sizeof(req));
        resp.code = -1;
        snprintf(resp.message, sizeof(resp.message), "invalid request");
        write_full(client_fd, &resp, sizeof(resp));
        return -1;
    }

    LOG_D("IPC request: cmd=%d service='%s'", req.cmd, req.service_name);

    if (req.service_name[0] != '\0') {
        svc = service_find(req.service_name);
    }

    switch (req.cmd) {
    case CMD_START:
        if (!svc) {
            resp.code = -1;
            snprintf(resp.message,
                     sizeof(resp.message),
                     "service '%s' not found",
                     req.service_name);
        } else {
            resp.code = service_start(svc);
            snprintf(resp.message,
                     sizeof(resp.message),
                     resp.code == 0 ? "started" : "start failed");
        }
        break;

    case CMD_STOP:
        if (!svc) {
            resp.code = -1;
            snprintf(resp.message,
                     sizeof(resp.message),
                     "service '%s' not found",
                     req.service_name);
        } else {
            resp.code = service_stop(svc);
            snprintf(resp.message,
                     sizeof(resp.message),
                     resp.code == 0 ? "stopped" : "stop failed");
        }
        break;

    case CMD_RESTART:
        if (!svc) {
            resp.code = -1;
            snprintf(resp.message,
                     sizeof(resp.message),
                     "service '%s' not found",
                     req.service_name);
        } else {
            resp.code = service_restart(svc);
            snprintf(resp.message,
                     sizeof(resp.message),
                     resp.code == 0 ? "restarted" : "restart failed");
        }
        break;

    case CMD_STATUS:
        if (!svc) {
            resp.code = -1;
            snprintf(resp.message,
                     sizeof(resp.message),
                     "service '%s' not found",
                     req.service_name);
        } else {
            resp.code = 0;
            resp.state = svc->state;
            resp.pid = svc->pid;
            resp.exit_code = svc->exit_code;
            if (svc->state == SVC_STATE_RUNNING && svc->start_time > 0) {
                resp.uptime = util_monotonic_ns() - svc->start_time;
            }
            snprintf(resp.message,
                     sizeof(resp.message),
                     "%s",
                     service_state_str(svc->state));
        }
        break;

    case CMD_STATUS_ALL: {
        service_t *iter;
        int count = service_get_count();

        resp.code = count;
        snprintf(resp.message, sizeof(resp.message), "%d services loaded", count);
        write_full(client_fd, &resp, sizeof(resp));

        iter = service_get_list();
        while (iter) {
            memset(&resp, 0, sizeof(resp));
            resp.state = iter->state;
            resp.pid = iter->pid;
            resp.exit_code = iter->exit_code;
            if (iter->state == SVC_STATE_RUNNING && iter->start_time > 0) {
                resp.uptime = util_monotonic_ns() - iter->start_time;
            }
            snprintf(resp.message, sizeof(resp.message), "%s", iter->name);
            write_full(client_fd, &resp, sizeof(resp));
            iter = iter->next;
        }
        return 0;
    }

    case CMD_ENABLE:
        if (!svc) {
            resp.code = -1;
            snprintf(resp.message,
                     sizeof(resp.message),
                     "service '%s' not found",
                     req.service_name);
        } else {
            svc->enabled = true;
            resp.code = 0;
            snprintf(resp.message, sizeof(resp.message), "enabled");
        }
        break;

    case CMD_DISABLE:
        if (!svc) {
            resp.code = -1;
            snprintf(resp.message,
                     sizeof(resp.message),
                     "service '%s' not found",
                     req.service_name);
        } else {
            svc->enabled = false;
            resp.code = 0;
            snprintf(resp.message, sizeof(resp.message), "disabled");
        }
        break;

    case CMD_EVENT_PUBLISH:
        if (req.channel[0] == '\0' || req.event_type[0] == '\0') {
            resp.code = -1;
            snprintf(resp.message,
                     sizeof(resp.message),
                     "event channel/type required");
        } else {
            event_id = record_event(&req);
            resp.code = 0;
            copy_string(resp.message, sizeof(resp.message), "event published");
            copy_string(resp.channel, sizeof(resp.channel), req.channel);
            copy_string(resp.event_type, sizeof(resp.event_type), req.event_type);
            copy_string(resp.source, sizeof(resp.source), req.source);
            copy_string(resp.payload, sizeof(resp.payload), req.payload);
            resp.event_id = event_id;
            LOG_I("event published: channel=%s type=%s source=%s",
                  req.channel,
                  req.event_type,
                  req.source[0] != '\0' ? req.source : "(unknown)");
        }
        break;

    case CMD_EVENT_LIST: {
        int count = g_event_count;

        resp.code = count;
        snprintf(resp.message, sizeof(resp.message), "%d events buffered", count);
        write_full(client_fd, &resp, sizeof(resp));

        for (int i = 0; i < count; i++) {
            int index = (g_event_next - count + i + LUMID_MAX_EVENTS) %
                        LUMID_MAX_EVENTS;

            memset(&resp, 0, sizeof(resp));
            fill_event_response(&resp, &g_events[index]);
            write_full(client_fd, &resp, sizeof(resp));
        }
        return 0;
    }

    case CMD_POWEROFF:
        LOG_I("%s", "received poweroff command via IPC");
        resp.code = 0;
        snprintf(resp.message, sizeof(resp.message), "powering off");
        write_full(client_fd, &resp, sizeof(resp));
        kill(getpid(), SIGTERM);
        return 0;

    case CMD_REBOOT:
        LOG_I("%s", "received reboot command via IPC");
        resp.code = 0;
        snprintf(resp.message, sizeof(resp.message), "rebooting");
        write_full(client_fd, &resp, sizeof(resp));
        kill(getpid(), SIGTERM);
        return 0;

    default:
        resp.code = -1;
        snprintf(resp.message, sizeof(resp.message), "unknown command %d", req.cmd);
        break;
    }

    write_full(client_fd, &resp, sizeof(resp));
    return 0;
}
#endif

int socket_client_connect(const char *path)
{
    int fd;
    struct sockaddr_un addr;

    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        fprintf(stderr, "socket creation failed: %s\n", strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "failed to connect to %s: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int socket_send_request(int fd, const ipc_request_t *req)
{
    if (write_full(fd, req, sizeof(*req)) < 0) {
        fprintf(stderr, "failed to send request: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int socket_recv_response(int fd, ipc_response_t *resp)
{
    if (read_full(fd, resp, sizeof(*resp)) < 0) {
        fprintf(stderr, "failed to receive response: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}
