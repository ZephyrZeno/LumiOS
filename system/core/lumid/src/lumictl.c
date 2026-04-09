/*
 * lumictl.c - lumid control client
 *
 * Command-line tool to interact with the lumid init daemon.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lumid.h"

static void usage(const char *prog)
{
    fprintf(stderr,
            "lumictl - LumiOS service manager client\n"
            "\n"
            "Usage: %s <command> [args]\n"
            "\n"
            "Commands:\n"
            "  start   <service>  Start a service\n"
            "  stop    <service>  Stop a service\n"
            "  restart <service>  Restart a service\n"
            "  status  [service]  Show status (all if no service specified)\n"
            "  enable  <service>  Enable auto-start on boot\n"
            "  disable <service>  Disable auto-start on boot\n"
            "  log     <service>  Show service log\n"
            "  event publish <channel> <type> [source] [payload]\n"
            "                    Publish an event to lumid\n"
            "  event list         Show buffered events\n"
            "  poweroff           Power off the system\n"
            "  reboot             Reboot the system\n",
            prog);
}

static void format_uptime(uint64_t ns, char *buf, size_t size)
{
    uint64_t sec = ns / 1000000000ULL;
    uint64_t min = sec / 60;
    uint64_t hr = min / 60;
    uint64_t day = hr / 24;

    if (day > 0) {
        snprintf(buf,
                 size,
                 "%lud %luh %lum %lus",
                 (unsigned long)day,
                 (unsigned long)(hr % 24),
                 (unsigned long)(min % 60),
                 (unsigned long)(sec % 60));
    } else if (hr > 0) {
        snprintf(buf,
                 size,
                 "%luh %lum %lus",
                 (unsigned long)hr,
                 (unsigned long)(min % 60),
                 (unsigned long)(sec % 60));
    } else if (min > 0) {
        snprintf(buf,
                 size,
                 "%lum %lus",
                 (unsigned long)min,
                 (unsigned long)(sec % 60));
    } else {
        snprintf(buf, size, "%lus", (unsigned long)sec);
    }
}

static const char *status_state_str(svc_state_t state)
{
    switch (state) {
    case SVC_STATE_STOPPED:
        return "stopped";
    case SVC_STATE_STARTING:
        return "starting";
    case SVC_STATE_RUNNING:
        return "running";
    case SVC_STATE_STOPPING:
        return "stopping";
    case SVC_STATE_FAILED:
        return "failed";
    case SVC_STATE_RESTARTING:
        return "restarting";
    default:
        return "unknown";
    }
}

static void print_status(const char *name, const ipc_response_t *resp)
{
    const char *color = "";
    const char *reset = "";

    if (isatty(STDOUT_FILENO)) {
        reset = "\033[0m";
        switch (resp->state) {
        case SVC_STATE_RUNNING:
            color = "\033[32m";
            break;
        case SVC_STATE_FAILED:
            color = "\033[31m";
            break;
        case SVC_STATE_RESTARTING:
            color = "\033[33m";
            break;
        default:
            color = "\033[37m";
            break;
        }
    }

    printf("  %-24s %s%-12s%s", name, color, status_state_str(resp->state), reset);

    if (resp->state == SVC_STATE_RUNNING && resp->pid > 0) {
        char uptime_str[64] = "";

        if (resp->uptime > 0) {
            format_uptime(resp->uptime, uptime_str, sizeof(uptime_str));
        }
        printf("  pid=%-6d  uptime=%s", resp->pid, uptime_str);
    } else if ((resp->state == SVC_STATE_FAILED ||
                resp->state == SVC_STATE_STOPPED) &&
               resp->exit_code != -1) {
        printf("  exit=%d", resp->exit_code);
    }

    printf("\n");
}

static void print_event(const ipc_response_t *resp)
{
    char age[64];

    if (!resp) {
        return;
    }

    format_uptime(resp->uptime, age, sizeof(age));
    printf("  #%-6llu %-12s %-28s %-24s %-12s %s\n",
           (unsigned long long)resp->event_id,
           resp->channel[0] != '\0' ? resp->channel : "-",
           resp->event_type[0] != '\0' ? resp->event_type : "-",
           resp->source[0] != '\0' ? resp->source : "-",
           age,
            resp->payload[0] != '\0' ? resp->payload : "-");
}

static void join_args(char *buffer,
                      size_t size,
                      int argc,
                      char *argv[],
                      int start_index)
{
    size_t used = 0;

    if (!buffer || size == 0) {
        return;
    }

    buffer[0] = '\0';
    for (int i = start_index; i < argc; i++) {
        size_t chunk = strlen(argv[i]);

        if (used > 0) {
            if (used + 1 >= size) {
                break;
            }
            buffer[used++] = ' ';
        }

        if (used + chunk >= size) {
            chunk = size - used - 1;
        }
        memcpy(buffer + used, argv[i], chunk);
        used += chunk;

        if (used + 1 >= size) {
            break;
        }
    }

    buffer[used] = '\0';
}

static int show_log(const char *service_name)
{
    char path[LUMID_MAX_PATH_LEN];
    char *content;
    char *p;
    int line_count = 0;

    snprintf(path, sizeof(path), "%s/%s.log", LUMID_LOG_DIR, service_name);

    content = util_read_file(path);
    if (!content) {
        fprintf(stderr, "No log found for service '%s'\n", service_name);
        return -1;
    }

    p = content;
    while (*p && line_count < 50) {
        line_count++;
        p = strchr(p, '\n');
        if (p) {
            p++;
        } else {
            break;
        }
    }

    if (line_count >= 50) {
        char *last = content + strlen(content);
        int found = 0;

        for (p = last - 1; p > content && found < 50; p--) {
            if (*p == '\n') {
                found++;
            }
        }
        if (found >= 50) {
            p += 2;
        }
        printf("%s", p);
    } else {
        printf("%s", content);
    }

    free(content);
    return 0;
}

static bool parse_event_command(int argc, char *argv[], ipc_request_t *req)
{
    const char *subcmd = (argc > 2) ? argv[2] : "";

    if (strcmp(subcmd, "publish") == 0) {
        req->cmd = CMD_EVENT_PUBLISH;
        if (argc < 5) {
            fprintf(stderr,
                    "Usage: lumictl event publish <channel> <type> [source] [payload]\n");
            return false;
        }

        strncpy(req->channel, argv[3], sizeof(req->channel) - 1);
        strncpy(req->event_type, argv[4], sizeof(req->event_type) - 1);
        if (argc > 5) {
            strncpy(req->source, argv[5], sizeof(req->source) - 1);
        }
        if (argc > 6) {
            join_args(req->payload, sizeof(req->payload), argc, argv, 6);
        }
        return true;
    }

    if (strcmp(subcmd, "list") == 0) {
        req->cmd = CMD_EVENT_LIST;
        return true;
    }

    fprintf(stderr, "Usage: lumictl event <publish|list> ...\n");
    return false;
}

int main(int argc, char *argv[])
{
    ipc_request_t req;
    ipc_response_t resp;
    const char *cmd;
    const char *svc_name = "";
    int fd;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    cmd = argv[1];
    memset(&req, 0, sizeof(req));

    if (strcmp(cmd, "log") == 0) {
        svc_name = (argc > 2) ? argv[2] : "";
        if (svc_name[0] == '\0') {
            fprintf(stderr, "Usage: lumictl log <service>\n");
            return 1;
        }
        return show_log(svc_name);
    }

    if (strcmp(cmd, "start") == 0) {
        req.cmd = CMD_START;
        svc_name = (argc > 2) ? argv[2] : "";
        if (svc_name[0] == '\0') {
            fprintf(stderr, "Usage: lumictl start <service>\n");
            return 1;
        }
    } else if (strcmp(cmd, "stop") == 0) {
        req.cmd = CMD_STOP;
        svc_name = (argc > 2) ? argv[2] : "";
        if (svc_name[0] == '\0') {
            fprintf(stderr, "Usage: lumictl stop <service>\n");
            return 1;
        }
    } else if (strcmp(cmd, "restart") == 0) {
        req.cmd = CMD_RESTART;
        svc_name = (argc > 2) ? argv[2] : "";
        if (svc_name[0] == '\0') {
            fprintf(stderr, "Usage: lumictl restart <service>\n");
            return 1;
        }
    } else if (strcmp(cmd, "status") == 0) {
        svc_name = (argc > 2) ? argv[2] : "";
        req.cmd = (svc_name[0] != '\0') ? CMD_STATUS : CMD_STATUS_ALL;
    } else if (strcmp(cmd, "enable") == 0) {
        req.cmd = CMD_ENABLE;
        svc_name = (argc > 2) ? argv[2] : "";
        if (svc_name[0] == '\0') {
            fprintf(stderr, "Usage: lumictl enable <service>\n");
            return 1;
        }
    } else if (strcmp(cmd, "disable") == 0) {
        req.cmd = CMD_DISABLE;
        svc_name = (argc > 2) ? argv[2] : "";
        if (svc_name[0] == '\0') {
            fprintf(stderr, "Usage: lumictl disable <service>\n");
            return 1;
        }
    } else if (strcmp(cmd, "event") == 0) {
        if (!parse_event_command(argc, argv, &req)) {
            return 1;
        }
    } else if (strcmp(cmd, "poweroff") == 0) {
        req.cmd = CMD_POWEROFF;
    } else if (strcmp(cmd, "reboot") == 0) {
        req.cmd = CMD_REBOOT;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        usage(argv[0]);
        return 1;
    }

    if (svc_name[0] != '\0') {
        strncpy(req.service_name, svc_name, sizeof(req.service_name) - 1);
    }

    fd = socket_client_connect(LUMID_SOCKET_PATH);
    if (fd < 0) {
        fprintf(stderr, "Failed to connect to lumid. Is the system running?\n");
        return 1;
    }

    if (socket_send_request(fd, &req) < 0) {
        close(fd);
        return 1;
    }

    if (socket_recv_response(fd, &resp) < 0) {
        close(fd);
        return 1;
    }

    if (req.cmd == CMD_STATUS_ALL) {
        int count = resp.code;

        printf("LumiOS Services (%d loaded):\n", count);
        printf("  %-24s %-12s  %s\n", "NAME", "STATE", "INFO");
        printf("  %-24s %-12s  %s\n", "----", "-----", "----");

        for (int i = 0; i < count; i++) {
            ipc_response_t svc_resp;

            if (socket_recv_response(fd, &svc_resp) < 0) {
                break;
            }
            print_status(svc_resp.message, &svc_resp);
        }
    } else if (req.cmd == CMD_STATUS) {
        if (resp.code < 0) {
            fprintf(stderr, "%s\n", resp.message);
            close(fd);
            return 1;
        }

        printf("Service: %s\n", svc_name);
        print_status(svc_name, &resp);
    } else if (req.cmd == CMD_EVENT_LIST) {
        int count = resp.code;

        if (count < 0) {
            fprintf(stderr, "%s\n", resp.message);
            close(fd);
            return 1;
        }

        printf("Buffered events (%d):\n", count);
        printf("  %-7s %-12s %-28s %-24s %-12s %s\n",
               "ID",
               "CHANNEL",
               "TYPE",
               "SOURCE",
               "AGE",
               "PAYLOAD");
        printf("  %-7s %-12s %-28s %-24s %-12s %s\n",
               "--",
               "-------",
               "----",
               "------",
               "---",
               "-------");

        for (int i = 0; i < count; i++) {
            ipc_response_t event_resp;

            if (socket_recv_response(fd, &event_resp) < 0) {
                close(fd);
                return 1;
            }
            print_event(&event_resp);
        }
    } else if (req.cmd == CMD_EVENT_PUBLISH) {
        if (resp.code < 0) {
            fprintf(stderr, "Error: %s\n", resp.message);
            close(fd);
            return 1;
        }

        printf("Published event %s/%s",
               resp.channel[0] != '\0' ? resp.channel : "-",
               resp.event_type[0] != '\0' ? resp.event_type : "-");
        if (resp.event_id > 0) {
            printf(" #%llu", (unsigned long long)resp.event_id);
        }
        if (resp.source[0] != '\0') {
            printf(" from %s", resp.source);
        }
        if (resp.payload[0] != '\0') {
            printf(": %s", resp.payload);
        }
        printf("\n");
    } else {
        if (resp.code < 0) {
            fprintf(stderr, "Error: %s\n", resp.message);
            close(fd);
            return 1;
        }
        printf("%s\n", resp.message);
    }

    close(fd);
    return 0;
}
