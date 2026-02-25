/*
 * lumictl.c - lumid control client / lumid 控制客户端
 *
 * Command-line tool to interact with the lumid init daemon.
 * 与 lumid 守护进程交互的命令行工具。
 *
 * Usage / 用法:
 *   lumictl start <service>
 *   lumictl stop <service>
 *   lumictl restart <service>
 *   lumictl status [service]
 *   lumictl enable <service>
 *   lumictl disable <service>
 *   lumictl log <service>
 *   lumictl poweroff
 *   lumictl reboot
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lumid.h"

/* === Print usage / 打印用法 === */

static void usage(const char *prog)
{
    fprintf(stderr,
        "lumictl - LumiOS service manager client\n"
        "\n"
        "Usage: %s <command> [service]\n"
        "\n"
        "Commands:\n"
        "  start   <service>  Start a service\n"
        "  stop    <service>  Stop a service\n"
        "  restart <service>  Restart a service\n"
        "  status  [service]  Show status (all if no service specified)\n"
        "  enable  <service>  Enable auto-start on boot\n"
        "  disable <service>  Disable auto-start on boot\n"
        "  log     <service>  Show service log\n"
        "  poweroff           Power off the system\n"
        "  reboot             Reboot the system\n",
        prog);
}

/* === Format uptime to human-readable / 格式化运行时间为可读形式 === */

static void format_uptime(uint64_t ns, char *buf, size_t size)
{
    uint64_t sec = ns / 1000000000ULL;
    uint64_t min = sec / 60;
    uint64_t hr  = min / 60;
    uint64_t day = hr / 24;

    if (day > 0) {
        snprintf(buf, size, "%lud %luh %lum %lus",
                 (unsigned long)day, (unsigned long)(hr % 24),
                 (unsigned long)(min % 60), (unsigned long)(sec % 60));
    } else if (hr > 0) {
        snprintf(buf, size, "%luh %lum %lus",
                 (unsigned long)hr, (unsigned long)(min % 60),
                 (unsigned long)(sec % 60));
    } else if (min > 0) {
        snprintf(buf, size, "%lum %lus",
                 (unsigned long)min, (unsigned long)(sec % 60));
    } else {
        snprintf(buf, size, "%lus", (unsigned long)sec);
    }
}

/* === Print single service status / 打印单个服务状态 === */

static void print_status(const char *name, const ipc_response_t *resp)
{
    /* Color based on state / 根据状态着色 */
    const char *color = "";
    const char *reset = "";

    if (isatty(STDOUT_FILENO)) {
        reset = "\033[0m";
        switch (resp->state) {
        case SVC_STATE_RUNNING:    color = "\033[32m"; break; /* green / 绿色 */
        case SVC_STATE_FAILED:     color = "\033[31m"; break; /* red / 红色 */
        case SVC_STATE_RESTARTING: color = "\033[33m"; break; /* yellow / 黄色 */
        default:                   color = "\033[37m"; break; /* gray / 灰色 */
        }
    }

    printf("  %-24s %s%-12s%s", name, color,
           service_state_str(resp->state), reset);

    if (resp->state == SVC_STATE_RUNNING && resp->pid > 0) {
        char uptime_str[64] = "";
        if (resp->uptime > 0)
            format_uptime(resp->uptime, uptime_str, sizeof(uptime_str));
        printf("  pid=%-6d  uptime=%s", resp->pid, uptime_str);
    } else if (resp->state == SVC_STATE_FAILED ||
               resp->state == SVC_STATE_STOPPED) {
        if (resp->exit_code != -1) {
            printf("  exit=%d", resp->exit_code);
        }
    }

    printf("\n");
}

/* === Show service log file / 显示服务日志文件 === */

static int show_log(const char *service_name)
{
    char path[LUMID_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s.log", LUMID_LOG_DIR, service_name);

    char *content = util_read_file(path);
    if (!content) {
        fprintf(stderr, "No log found for service '%s'\n", service_name);
        return -1;
    }

    /* Print last 50 lines / 打印最后 50 行 */
    char *lines[50];
    int line_count = 0;
    char *p = content;
    while (*p && line_count < 50) {
        lines[line_count++] = p;
        p = strchr(p, '\n');
        if (p) p++;
        else break;
    }

    /* If more than 50 lines, find the last 50 / 如果超过 50 行，找到最后 50 行 */
    if (line_count >= 50) {
        line_count = 0;
        char *last = content + strlen(content);
        int found = 0;
        for (p = last - 1; p > content && found < 50; p--) {
            if (*p == '\n')
                found++;
        }
        if (found >= 50)
            p += 2;
        printf("%s", p);
    } else {
        printf("%s", content);
    }

    free(content);
    return 0;
}

/* === Entry point / 入口点 === */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];
    const char *svc_name = (argc > 2) ? argv[2] : "";

    /* Handle log command locally / 本地处理 log 命令 */
    if (strcmp(cmd, "log") == 0) {
        if (svc_name[0] == '\0') {
            fprintf(stderr, "Usage: lumictl log <service>\n");
            return 1;
        }
        return show_log(svc_name);
    }

    /* Build IPC request / 构建 IPC 请求 */
    ipc_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.service_name, svc_name, LUMID_MAX_NAME_LEN - 1);

    if (strcmp(cmd, "start") == 0) {
        req.cmd = CMD_START;
        if (svc_name[0] == '\0') {
            fprintf(stderr, "Usage: lumictl start <service>\n");
            return 1;
        }
    } else if (strcmp(cmd, "stop") == 0) {
        req.cmd = CMD_STOP;
        if (svc_name[0] == '\0') {
            fprintf(stderr, "Usage: lumictl stop <service>\n");
            return 1;
        }
    } else if (strcmp(cmd, "restart") == 0) {
        req.cmd = CMD_RESTART;
        if (svc_name[0] == '\0') {
            fprintf(stderr, "Usage: lumictl restart <service>\n");
            return 1;
        }
    } else if (strcmp(cmd, "status") == 0) {
        req.cmd = (svc_name[0] != '\0') ? CMD_STATUS : CMD_STATUS_ALL;
    } else if (strcmp(cmd, "enable") == 0) {
        req.cmd = CMD_ENABLE;
    } else if (strcmp(cmd, "disable") == 0) {
        req.cmd = CMD_DISABLE;
    } else if (strcmp(cmd, "poweroff") == 0) {
        req.cmd = CMD_POWEROFF;
    } else if (strcmp(cmd, "reboot") == 0) {
        req.cmd = CMD_REBOOT;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        usage(argv[0]);
        return 1;
    }

    /* Connect to lumid / 连接到 lumid */
    int fd = socket_client_connect(LUMID_SOCKET_PATH);
    if (fd < 0) {
        fprintf(stderr, "Failed to connect to lumid. Is the system running?\n");
        return 1;
    }

    /* Send request / 发送请求 */
    if (socket_send_request(fd, &req) < 0) {
        close(fd);
        return 1;
    }

    /* Receive response / 接收响应 */
    ipc_response_t resp;
    if (socket_recv_response(fd, &resp) < 0) {
        close(fd);
        return 1;
    }

    /* Handle response based on command / 根据命令处理响应 */
    if (req.cmd == CMD_STATUS_ALL) {
        int count = resp.code;
        printf("LumiOS Services (%d loaded):\n", count);
        printf("  %-24s %-12s  %s\n", "NAME", "STATE", "INFO");
        printf("  %-24s %-12s  %s\n", "----", "-----", "----");

        /* Read individual service statuses / 读取各服务状态 */
        for (int i = 0; i < count; i++) {
            ipc_response_t svc_resp;
            if (socket_recv_response(fd, &svc_resp) < 0)
                break;
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
    } else {
        /* Generic command response / 通用命令响应 */
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
