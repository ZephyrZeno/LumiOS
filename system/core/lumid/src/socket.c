/*
 * socket.c - IPC communication via Unix domain socket / 通过 Unix 域套接字的 IPC 通信
 *
 * Provides server/client API for lumictl <-> lumid communication.
 * 提供 lumictl 与 lumid 之间的服务端/客户端通信 API。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lumid.h"

/* === Server: create and bind listening socket / 服务端: 创建并绑定监听套接字 === */

int socket_server_init(const char *path)
{
    int fd;
    struct sockaddr_un addr;

    /* Remove stale socket file / 移除残留的套接字文件 */
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

    /* Allow all users to connect / 允许所有用户连接 */
    chmod(path, 0666);

    if (listen(fd, 8) < 0) {
        LOG_E("socket listen failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    LOG_D("IPC socket listening on %s (fd %d)", path, fd);
    return fd;
}

/* === Server: accept a client connection / 服务端: 接受客户端连接 === */

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

/* === Server: handle an IPC request / 服务端: 处理 IPC 请求 === */

int socket_handle_request(int client_fd)
{
    ipc_request_t req;
    ipc_response_t resp;
    ssize_t n;

    memset(&resp, 0, sizeof(resp));

    n = read(client_fd, &req, sizeof(req));
    if (n != sizeof(req)) {
        LOG_W("malformed IPC request (got %zd bytes, expected %zu)",
              n, sizeof(req));
        resp.code = -1;
        snprintf(resp.message, sizeof(resp.message), "invalid request");
        write(client_fd, &resp, sizeof(resp));
        return -1;
    }

    LOG_D("IPC request: cmd=%d service='%s'", req.cmd, req.service_name);

    service_t *svc = NULL;
    if (req.service_name[0] != '\0') {
        svc = service_find(req.service_name);
    }

    switch (req.cmd) {
    case CMD_START:
        if (!svc) {
            resp.code = -1;
            snprintf(resp.message, sizeof(resp.message),
                     "service '%s' not found", req.service_name);
        } else {
            resp.code = service_start(svc);
            snprintf(resp.message, sizeof(resp.message),
                     resp.code == 0 ? "started" : "start failed");
        }
        break;

    case CMD_STOP:
        if (!svc) {
            resp.code = -1;
            snprintf(resp.message, sizeof(resp.message),
                     "service '%s' not found", req.service_name);
        } else {
            resp.code = service_stop(svc);
            snprintf(resp.message, sizeof(resp.message),
                     resp.code == 0 ? "stopped" : "stop failed");
        }
        break;

    case CMD_RESTART:
        if (!svc) {
            resp.code = -1;
            snprintf(resp.message, sizeof(resp.message),
                     "service '%s' not found", req.service_name);
        } else {
            resp.code = service_restart(svc);
            snprintf(resp.message, sizeof(resp.message),
                     resp.code == 0 ? "restarted" : "restart failed");
        }
        break;

    case CMD_STATUS:
        if (!svc) {
            resp.code = -1;
            snprintf(resp.message, sizeof(resp.message),
                     "service '%s' not found", req.service_name);
        } else {
            resp.code = 0;
            resp.state = svc->state;
            resp.pid = svc->pid;
            resp.exit_code = svc->exit_code;
            if (svc->state == SVC_STATE_RUNNING && svc->start_time > 0) {
                resp.uptime = util_monotonic_ns() - svc->start_time;
            }
            snprintf(resp.message, sizeof(resp.message), "%s",
                     service_state_str(svc->state));
        }
        break;

    case CMD_STATUS_ALL: {
        /* Return count in code, details via multiple writes / 通过 code 返回数量 */
        int count = service_get_count();
        resp.code = count;
        snprintf(resp.message, sizeof(resp.message),
                 "%d services loaded", count);
        write(client_fd, &resp, sizeof(resp));

        /* Send individual service status / 发送各服务状态 */
        service_t *iter = service_get_list();
        while (iter) {
            memset(&resp, 0, sizeof(resp));
            resp.state = iter->state;
            resp.pid = iter->pid;
            resp.exit_code = iter->exit_code;
            if (iter->state == SVC_STATE_RUNNING && iter->start_time > 0) {
                resp.uptime = util_monotonic_ns() - iter->start_time;
            }
            snprintf(resp.message, sizeof(resp.message), "%s", iter->name);
            write(client_fd, &resp, sizeof(resp));
            iter = iter->next;
        }
        return 0; /* Already sent response / 已发送响应 */
    }

    case CMD_ENABLE:
        if (svc) {
            svc->enabled = true;
            resp.code = 0;
            snprintf(resp.message, sizeof(resp.message), "enabled");
        }
        break;

    case CMD_DISABLE:
        if (svc) {
            svc->enabled = false;
            resp.code = 0;
            snprintf(resp.message, sizeof(resp.message), "disabled");
        }
        break;

    case CMD_POWEROFF:
        LOG_I("received poweroff command via IPC");
        resp.code = 0;
        snprintf(resp.message, sizeof(resp.message), "powering off");
        write(client_fd, &resp, sizeof(resp));
        /* Signal main loop to exit / 通知主循环退出 */
        kill(getpid(), SIGTERM);
        return 0;

    case CMD_REBOOT:
        LOG_I("received reboot command via IPC");
        resp.code = 0;
        snprintf(resp.message, sizeof(resp.message), "rebooting");
        write(client_fd, &resp, sizeof(resp));
        kill(getpid(), SIGTERM);
        return 0;

    default:
        resp.code = -1;
        snprintf(resp.message, sizeof(resp.message), "unknown command %d",
                 req.cmd);
        break;
    }

    write(client_fd, &resp, sizeof(resp));
    return 0;
}

/* === Client: connect to lumid socket / 客户端: 连接到 lumid 套接字 === */

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
        fprintf(stderr, "failed to connect to %s: %s\n",
                path, strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

/* === Client: send request / 客户端: 发送请求 === */

int socket_send_request(int fd, const ipc_request_t *req)
{
    ssize_t n = write(fd, req, sizeof(*req));
    if (n != sizeof(*req)) {
        fprintf(stderr, "failed to send request: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/* === Client: receive response / 客户端: 接收响应 === */

int socket_recv_response(int fd, ipc_response_t *resp)
{
    ssize_t n = read(fd, resp, sizeof(*resp));
    if (n != sizeof(*resp)) {
        fprintf(stderr, "failed to receive response: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}
