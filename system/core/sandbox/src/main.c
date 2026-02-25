/*
 * main.c - Sandbox CLI tool / 沙箱命令行工具
 *
 * Usage: lumi-sandbox [--level LEVEL] [--policy FILE] -- COMMAND [ARGS...]
 * 用法: lumi-sandbox [--level 等级] [--policy 文件] -- 命令 [参数...]
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sandbox.h"

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [OPTIONS] -- COMMAND [ARGS...]\n"
        "\n"
        "Options:\n"
        "  --level LEVEL    Security level: none/trusted/normal/strict/untrusted\n"
        "  --policy FILE    Load policy from file\n"
        "  --test           Test sandbox support and exit\n"
        "  -h, --help       Show this help\n"
        "\n", prog);
}

static sbx_level_t parse_level(const char *s)
{
    if (strcmp(s, "none") == 0)       return SBX_LEVEL_NONE;
    if (strcmp(s, "trusted") == 0)    return SBX_LEVEL_TRUSTED;
    if (strcmp(s, "normal") == 0)     return SBX_LEVEL_NORMAL;
    if (strcmp(s, "strict") == 0)     return SBX_LEVEL_STRICT;
    if (strcmp(s, "untrusted") == 0)  return SBX_LEVEL_UNTRUSTED;
    return SBX_LEVEL_NORMAL;
}

int main(int argc, char *argv[])
{
    sbx_policy_t policy;
    sbx_level_t level = SBX_LEVEL_NORMAL;
    const char *policy_file = NULL;
    int cmd_start = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            cmd_start = i + 1;
            break;
        } else if (strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
            level = parse_level(argv[++i]);
        } else if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc) {
            policy_file = argv[++i];
        } else if (strcmp(argv[i], "--test") == 0) {
            sbx_init();
            return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    if (cmd_start < 0 || cmd_start >= argc) {
        usage(argv[0]);
        return 1;
    }

    sbx_init();

    if (policy_file) {
        if (sbx_policy_load(&policy, policy_file) < 0) {
            fprintf(stderr, "[sandbox] failed to load policy: %s\n", policy_file);
            return 1;
        }
    } else {
        sbx_policy_default(&policy, level);
        snprintf(policy.name, sizeof(policy.name), "%s", argv[cmd_start]);
    }

    if (sbx_apply(&policy) < 0) {
        fprintf(stderr, "[sandbox] WARNING: some sandbox layers failed\n");
    }

    /* Execute the target command / 执行目标命令 */
    execvp(argv[cmd_start], &argv[cmd_start]);
    perror("execvp");
    return 127;
}
