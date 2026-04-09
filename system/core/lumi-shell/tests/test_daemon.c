#define _POSIX_C_SOURCE 200809L

#include "shell_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_SOCKET_CMD_EVENT_LIST 10
#define TEST_MAX_NAME_LEN 64
#define TEST_MAX_MESSAGE_LEN 256
#define TEST_MAX_TYPE_LEN 64
#define TEST_MAX_PAYLOAD_LEN 256

typedef struct {
    int cmd;
    char service_name[TEST_MAX_NAME_LEN];
    char channel[TEST_MAX_NAME_LEN];
    char event_type[TEST_MAX_TYPE_LEN];
    char source[TEST_MAX_NAME_LEN];
    char payload[TEST_MAX_PAYLOAD_LEN];
    int flags;
} test_request_t;

typedef struct {
    int code;
    char message[TEST_MAX_MESSAGE_LEN];
    char channel[TEST_MAX_NAME_LEN];
    char event_type[TEST_MAX_TYPE_LEN];
    char source[TEST_MAX_NAME_LEN];
    char payload[TEST_MAX_PAYLOAD_LEN];
    int state;
    int pid;
    int exit_code;
    unsigned long long uptime;
    unsigned long long event_id;
} test_response_t;

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

static void wait_for_socket(const char *socket_path)
{
    for (int attempt = 0; attempt < 50; attempt++) {
        struct stat st;
        struct timespec delay = {.tv_sec = 0, .tv_nsec = 20 * 1000 * 1000};

        if (stat(socket_path, &st) == 0) {
            return;
        }
        nanosleep(&delay, NULL);
    }

    fail("Timed out waiting for fake lumid socket");
}

static pid_t spawn_fake_lumid(const char *socket_path)
{
    pid_t child = fork();

    if (child < 0) {
        fail("fork failed");
    }
    if (child == 0) {
        int server_fd;
        int client_fd;
        struct sockaddr_un addr;
        test_request_t request;
        test_response_t header;
        test_response_t updated_event;
        test_response_t rejected_event;

        unlink(socket_path);

        server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd < 0) {
            _exit(10);
        }

        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        copy_string(addr.sun_path, sizeof(addr.sun_path), socket_path);

        if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            close(server_fd);
            _exit(11);
        }
        if (listen(server_fd, 1) != 0) {
            close(server_fd);
            unlink(socket_path);
            _exit(12);
        }

        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            close(server_fd);
            unlink(socket_path);
            _exit(13);
        }

        memset(&request, 0, sizeof(request));
        if (read_full(client_fd, &request, sizeof(request)) != 0 ||
            request.cmd != TEST_SOCKET_CMD_EVENT_LIST) {
            close(client_fd);
            close(server_fd);
            unlink(socket_path);
            _exit(14);
        }

        memset(&header, 0, sizeof(header));
        header.code = 2;
        copy_string(header.message, sizeof(header.message), "2 buffered event(s)");

        memset(&updated_event, 0, sizeof(updated_event));
        copy_string(updated_event.channel, sizeof(updated_event.channel), "system");
        copy_string(updated_event.event_type,
                    sizeof(updated_event.event_type),
                    "system.state.updated");
        copy_string(updated_event.source, sizeof(updated_event.source), "com.lumios.settings");
        copy_string(updated_event.payload,
                    sizeof(updated_event.payload),
                    "active_app=com.lumios.settings;status=Quick settings updated");
        updated_event.event_id = 41;

        memset(&rejected_event, 0, sizeof(rejected_event));
        copy_string(rejected_event.channel, sizeof(rejected_event.channel), "system");
        copy_string(rejected_event.event_type,
                    sizeof(rejected_event.event_type),
                    "system.state.rejected");
        copy_string(rejected_event.source, sizeof(rejected_event.source), "com.lumios.terminal");
        copy_string(rejected_event.payload,
                    sizeof(rejected_event.payload),
                    "status=Restricted key blocked");
        rejected_event.event_id = 42;

        if (write_full(client_fd, &header, sizeof(header)) != 0 ||
            write_full(client_fd, &updated_event, sizeof(updated_event)) != 0 ||
            write_full(client_fd, &rejected_event, sizeof(rejected_event)) != 0) {
            close(client_fd);
            close(server_fd);
            unlink(socket_path);
            _exit(15);
        }

        close(client_fd);
        close(server_fd);
        unlink(socket_path);
        _exit(0);
    }

    wait_for_socket(socket_path);
    return child;
}

static void reap_fake_lumid(pid_t child, const char *socket_path)
{
    int status = 0;

    waitpid(child, &status, 0);
    unlink(socket_path);
    expect(WIFEXITED(status) && WEXITSTATUS(status) == 0,
           "fake lumid did not exit cleanly");
}

static void test_daemon_bridge_success(void)
{
    char socket_path[108];
    pid_t child;
    lumi_shell_config_t config = {
        .screen_width = 1080,
        .screen_height = 2400,
        .lumid_socket_path = socket_path,
    };
    lumi_shell_override_t overrides = {0};
    lumi_shell_shared_state_t shared_state = {0};
    lumi_shell_notification_t notifications[MAX_NOTIFICATIONS] = {0};
    int notification_count = 0;
    uint64_t last_seen_event_id = 0;

    snprintf(socket_path, sizeof(socket_path), "/tmp/lumi-shell-daemon-%ld.sock", (long)getpid());
    child = spawn_fake_lumid(socket_path);

    lumi_shell_apply_daemon_events(&config,
                                   &overrides,
                                   &shared_state,
                                   notifications,
                                   &notification_count,
                                   MAX_NOTIFICATIONS,
                                   &last_seen_event_id);

    reap_fake_lumid(child, socket_path);

    expect(config.daemon_connected, "daemon bridge should be connected");
    expect(shared_state.daemon_connected, "shared state should record connected bridge");
    expect(config.daemon_event_count == 2, "bridge should report 2 buffered events");
    expect(last_seen_event_id == 42, "bridge cursor should track latest event id");
    expect(notification_count == 2, "bridge should surface both notifications");
    expect(strcmp(config.daemon_last_event_type, "system.state.rejected") == 0,
           "last daemon event type mismatch");
    expect(strcmp(config.daemon_last_event_source, "com.lumios.terminal") == 0,
           "last daemon event source mismatch");
    expect(strcmp(config.active_app, "com.lumios.settings") == 0,
           "active app should come from daemon payload");
    expect(strcmp(config.status_line, "Restricted key blocked") == 0,
           "status line should reflect latest daemon payload");
    expect(strstr(config.recent_action, "restricted system change") != NULL,
           "recent action should reflect rejected change");
    expect(strcmp(notifications[0].title, "System Update") == 0,
           "updated notification title mismatch");
    expect(strcmp(notifications[0].body, "Quick settings updated") == 0,
           "updated notification body mismatch");
    expect(strcmp(notifications[0].source, "com.lumios.settings") == 0,
           "updated notification source mismatch");
    expect(strcmp(notifications[1].title, "System Change Rejected") == 0,
           "rejected notification title mismatch");
    expect(strcmp(notifications[1].body, "Restricted key blocked") == 0,
           "rejected notification body mismatch");
    expect(strcmp(notifications[1].source, "com.lumios.terminal") == 0,
           "rejected notification source mismatch");
}

static void test_default_probe_is_silent(void)
{
    lumi_shell_config_t config = {
        .screen_width = 1080,
        .screen_height = 2400,
        .lumid_socket_path = NULL,
    };
    lumi_shell_override_t overrides = {0};
    lumi_shell_shared_state_t shared_state = {0};
    lumi_shell_notification_t notifications[MAX_NOTIFICATIONS] = {0};
    int notification_count = -1;
    uint64_t last_seen_event_id = 0;

    unsetenv("LUMI_LUMID_SOCKET_PATH");
    lumi_shell_apply_daemon_events(&config,
                                   &overrides,
                                   &shared_state,
                                   notifications,
                                   &notification_count,
                                   MAX_NOTIFICATIONS,
                                   &last_seen_event_id);

    expect(notification_count == 0, "default probe should not create notifications");
    expect(!config.daemon_connected, "default probe should not mark daemon connected");
    expect(config.lumid_socket_path == NULL, "default probe should stay silent when offline");
}

static void test_daemon_bridge_deduplicates_seen_events(void)
{
    char socket_path[108];
    pid_t child;
    lumi_shell_config_t config = {
        .screen_width = 1080,
        .screen_height = 2400,
        .lumid_socket_path = socket_path,
    };
    lumi_shell_override_t overrides = {0};
    lumi_shell_shared_state_t shared_state = {0};
    lumi_shell_notification_t notifications[MAX_NOTIFICATIONS] = {0};
    int notification_count = 0;
    uint64_t last_seen_event_id = 42;

    snprintf(socket_path, sizeof(socket_path), "/tmp/lumi-shell-daemon-%ld.sock", (long)getpid());
    child = spawn_fake_lumid(socket_path);

    lumi_shell_apply_daemon_events(&config,
                                   &overrides,
                                   &shared_state,
                                   notifications,
                                   &notification_count,
                                   MAX_NOTIFICATIONS,
                                   &last_seen_event_id);

    reap_fake_lumid(child, socket_path);

    expect(config.daemon_connected, "daemon bridge should still connect");
    expect(notification_count == 0, "seen events should not reappear as notifications");
    expect(last_seen_event_id == 42, "cursor should remain on latest seen event");
}

int main(void)
{
    test_daemon_bridge_success();
    test_default_probe_is_silent();
    test_daemon_bridge_deduplicates_seen_events();
    printf("daemon bridge tests passed\n");
    return 0;
}
