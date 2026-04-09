#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "lumid.h"

service_t *service_find(const char *name)
{
    (void)name;
    return NULL;
}

int service_start(service_t *svc)
{
    (void)svc;
    return -1;
}

int service_stop(service_t *svc)
{
    (void)svc;
    return -1;
}

int service_restart(service_t *svc)
{
    (void)svc;
    return -1;
}

const char *service_state_str(svc_state_t state)
{
    (void)state;
    return "unknown";
}

int service_get_count(void)
{
    return 0;
}

service_t *service_get_list(void)
{
    return NULL;
}

static void open_socket_pair(int fds[2])
{
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

    assert(rc == 0);
}

static void close_socket_pair(int fds[2])
{
    close(fds[0]);
    close(fds[1]);
}

static void test_event_publish_validation(void)
{
    int fds[2];
    ipc_request_t req;
    ipc_response_t resp;

    memset(&req, 0, sizeof(req));
    req.cmd = CMD_EVENT_PUBLISH;

    open_socket_pair(fds);
    assert(socket_send_request(fds[0], &req) == 0);
    assert(socket_handle_request(fds[1]) == 0);
    assert(socket_recv_response(fds[0], &resp) == 0);
    assert(resp.code == -1);
    assert(strcmp(resp.message, "event channel/type required") == 0);
    close_socket_pair(fds);
}

static void test_event_publish_and_list(void)
{
    int fds[2];
    ipc_request_t req;
    ipc_response_t resp;
    ipc_response_t event_resp;
    uint64_t published_event_id = 0;

    memset(&req, 0, sizeof(req));
    req.cmd = CMD_EVENT_PUBLISH;
    strncpy(req.channel, "system", sizeof(req.channel) - 1);
    strncpy(req.event_type, "system.state.updated", sizeof(req.event_type) - 1);
    strncpy(req.source, "com.lumios.settings", sizeof(req.source) - 1);
    strncpy(req.payload, "writer=settings;active_app=com.lumios.settings",
            sizeof(req.payload) - 1);

    open_socket_pair(fds);
    assert(socket_send_request(fds[0], &req) == 0);
    assert(socket_handle_request(fds[1]) == 0);
    assert(socket_recv_response(fds[0], &resp) == 0);
    assert(resp.code == 0);
    assert(strcmp(resp.channel, "system") == 0);
    assert(strcmp(resp.event_type, "system.state.updated") == 0);
    assert(strcmp(resp.source, "com.lumios.settings") == 0);
    assert(strcmp(resp.payload, "writer=settings;active_app=com.lumios.settings") == 0);
    assert(resp.event_id > 0);
    published_event_id = resp.event_id;
    close_socket_pair(fds);

    memset(&req, 0, sizeof(req));
    req.cmd = CMD_EVENT_LIST;

    open_socket_pair(fds);
    assert(socket_send_request(fds[0], &req) == 0);
    assert(socket_handle_request(fds[1]) == 0);
    assert(socket_recv_response(fds[0], &resp) == 0);
    assert(resp.code == 1);
    assert(strcmp(resp.message, "1 events buffered") == 0);
    assert(socket_recv_response(fds[0], &event_resp) == 0);
    assert(strcmp(event_resp.message, "event") == 0);
    assert(strcmp(event_resp.channel, "system") == 0);
    assert(strcmp(event_resp.event_type, "system.state.updated") == 0);
    assert(strcmp(event_resp.source, "com.lumios.settings") == 0);
    assert(strcmp(event_resp.payload,
                  "writer=settings;active_app=com.lumios.settings") == 0);
    assert(event_resp.uptime > 0);
    assert(event_resp.event_id == published_event_id);
    close_socket_pair(fds);
}

int main(void)
{
    printf("Running socket IPC tests...\n");
    log_init(NULL);
    log_set_level(LOG_DEBUG);

    test_event_publish_validation();
    test_event_publish_and_list();

    printf("socket IPC tests passed\n");
    return 0;
}
