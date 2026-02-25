/*
 * config.c - Service configuration parser / 服务配置解析器
 *
 * Parses .svc INI-style configuration files.
 * 解析 .svc INI 风格的配置文件。
 *
 * Format / 格式:
 *   [section]
 *   key = value
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "lumid.h"

/* === Helper: trim whitespace / 辅助: 去除空白 === */

static char *trim(char *str)
{
    while (isspace((unsigned char)*str))
        str++;

    if (*str == '\0')
        return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';

    return str;
}

/* === Parse key = value from a line / 从一行中解析 key = value === */

int config_parse_value(const char *line, char *key, char *value, size_t max)
{
    const char *eq = strchr(line, '=');
    if (!eq)
        return -1;

    /* Extract key / 提取键 */
    size_t key_len = (size_t)(eq - line);
    if (key_len >= max)
        key_len = max - 1;
    strncpy(key, line, key_len);
    key[key_len] = '\0';

    /* Trim key / 修剪键 */
    char *k = trim(key);
    if (k != key)
        memmove(key, k, strlen(k) + 1);

    /* Extract and trim value / 提取并修剪值 */
    const char *v = eq + 1;
    while (isspace((unsigned char)*v))
        v++;
    strncpy(value, v, max - 1);
    value[max - 1] = '\0';

    /* Remove trailing whitespace/newline / 移除尾部空白和换行 */
    size_t vlen = strlen(value);
    while (vlen > 0 && (value[vlen - 1] == '\n' || value[vlen - 1] == '\r' ||
                        isspace((unsigned char)value[vlen - 1]))) {
        value[--vlen] = '\0';
    }

    return 0;
}

/* === Parse restart policy string / 解析重启策略字符串 === */

static restart_policy_t parse_restart_policy(const char *str)
{
    if (strcmp(str, "always") == 0)       return RESTART_ALWAYS;
    if (strcmp(str, "on-failure") == 0)   return RESTART_ON_FAILURE;
    if (strcmp(str, "on-abnormal") == 0)  return RESTART_ON_ABNORMAL;
    return RESTART_NEVER;
}

/* === Parse service type string / 解析服务类型字符串 === */

static svc_type_t parse_service_type(const char *str)
{
    if (strcmp(str, "forking") == 0)   return SVC_TYPE_FORKING;
    if (strcmp(str, "oneshot") == 0)   return SVC_TYPE_ONESHOT;
    if (strcmp(str, "notify") == 0)    return SVC_TYPE_NOTIFY;
    return SVC_TYPE_SIMPLE;
}

/*
 * Parse space-separated list into array.
 * 将空格分隔的列表解析到数组中。
 */
static int parse_dep_list(const char *str, char out[][LUMID_MAX_NAME_LEN],
                          int max_count)
{
    int count = 0;
    char buf[LUMID_MAX_LINE_LEN];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, " \t,");
    while (tok && count < max_count) {
        strncpy(out[count], tok, LUMID_MAX_NAME_LEN - 1);
        out[count][LUMID_MAX_NAME_LEN - 1] = '\0';
        count++;
        tok = strtok(NULL, " \t,");
    }

    return count;
}

/* === Main config parser / 主配置解析函数 === */

int config_parse_service(const char *path, service_t *svc)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        LOG_E("failed to open config: %s: %s", path, strerror(errno));
        return -1;
    }

    char line[LUMID_MAX_LINE_LEN];
    char section[64] = "";
    char key[128], value[LUMID_MAX_PATH_LEN];
    int line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        char *l = trim(line);

        /* Skip empty lines and comments / 跳过空行和注释 */
        if (l[0] == '\0' || l[0] == '#' || l[0] == ';')
            continue;

        /* Section header / 段标题 */
        if (l[0] == '[') {
            char *end = strchr(l, ']');
            if (!end) {
                LOG_W("malformed section at %s:%d", path, line_num);
                continue;
            }
            *end = '\0';
            strncpy(section, l + 1, sizeof(section) - 1);
            continue;
        }

        /* Key = Value pair / 键值对 */
        if (config_parse_value(l, key, value, sizeof(value)) < 0) {
            LOG_W("malformed line at %s:%d: %s", path, line_num, l);
            continue;
        }

        /* === [service] section / [service] 段 === */
        if (strcmp(section, "service") == 0) {
            if (strcmp(key, "name") == 0) {
                strncpy(svc->name, value, LUMID_MAX_NAME_LEN - 1);
            } else if (strcmp(key, "description") == 0) {
                strncpy(svc->description, value, LUMID_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "exec") == 0) {
                strncpy(svc->exec_path, value, LUMID_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "args") == 0) {
                strncpy(svc->exec_args, value, LUMID_MAX_PATH_LEN - 1);
            } else if (strcmp(key, "type") == 0) {
                svc->type = parse_service_type(value);
            } else if (strcmp(key, "user") == 0) {
                strncpy(svc->user, value, LUMID_MAX_NAME_LEN - 1);
            } else if (strcmp(key, "group") == 0) {
                strncpy(svc->group, value, LUMID_MAX_NAME_LEN - 1);
            } else if (strcmp(key, "workdir") == 0) {
                strncpy(svc->working_dir, value, LUMID_MAX_PATH_LEN - 1);
            }
        }
        /* === [dependencies] section / [dependencies] 段 === */
        else if (strcmp(section, "dependencies") == 0) {
            if (strcmp(key, "requires") == 0) {
                svc->requires_count = parse_dep_list(
                    value, svc->requires, LUMID_MAX_DEPS);
            } else if (strcmp(key, "after") == 0) {
                svc->after_count = parse_dep_list(
                    value, svc->after, LUMID_MAX_DEPS);
            } else if (strcmp(key, "before") == 0) {
                svc->before_count = parse_dep_list(
                    value, svc->before, LUMID_MAX_DEPS);
            }
        }
        /* === [restart] section / [restart] 段 === */
        else if (strcmp(section, "restart") == 0) {
            if (strcmp(key, "policy") == 0) {
                svc->restart_policy = parse_restart_policy(value);
            } else if (strcmp(key, "delay") == 0) {
                svc->restart_delay_ms = atoi(value);
            } else if (strcmp(key, "max_retries") == 0) {
                svc->restart_max_retries = atoi(value);
            }
        }
        /* === [cgroup] section / [cgroup] 段 === */
        else if (strcmp(section, "cgroup") == 0) {
            if (strcmp(key, "memory_max") == 0) {
                util_parse_size(value, &svc->cgroup.memory_max);
            } else if (strcmp(key, "memory_high") == 0) {
                util_parse_size(value, &svc->cgroup.memory_high);
            } else if (strcmp(key, "cpu_weight") == 0) {
                svc->cgroup.cpu_weight = atoi(value);
            } else if (strcmp(key, "cpu_max") == 0) {
                svc->cgroup.cpu_max = atoi(value);
            } else if (strcmp(key, "io_weight") == 0) {
                svc->cgroup.io_weight = atoi(value);
            } else if (strcmp(key, "pids_max") == 0) {
                svc->cgroup.pids_max = atoi(value);
            }
        }
        /* === [environment] section / [environment] 段 === */
        else if (strcmp(section, "environment") == 0) {
            if (svc->env_count < LUMID_MAX_ENV) {
                strncpy(svc->env[svc->env_count].key, key,
                        LUMID_MAX_NAME_LEN - 1);
                strncpy(svc->env[svc->env_count].value, value,
                        LUMID_MAX_PATH_LEN - 1);
                svc->env_count++;
            }
        }
    }

    fclose(fp);

    /* Validate required fields / 验证必填字段 */
    if (svc->name[0] == '\0') {
        LOG_E("service config missing 'name': %s", path);
        return -1;
    }
    if (svc->exec_path[0] == '\0') {
        LOG_E("service '%s' missing 'exec': %s", svc->name, path);
        return -1;
    }

    LOG_D("parsed service '%s' from %s", svc->name, path);
    return 0;
}
