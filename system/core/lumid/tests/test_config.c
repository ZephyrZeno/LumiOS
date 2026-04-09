#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lumid.h"

void log_set_level(log_level_t level)
{
    (void)level;
}

void log_write(log_level_t level, const char *fmt, ...)
{
    va_list args;

    (void)level;
    va_start(args, fmt);
    va_end(args);
}

int util_parse_size(const char *str, int64_t *out)
{
    if (!str || !out) {
        return -1;
    }
    *out = 0;
    return 0;
}

static void fill_chars(char *dst, size_t len, char ch)
{
    size_t i;

    for (i = 0; i + 1 < len; ++i) {
        dst[i] = ch;
    }
    dst[len - 1] = '\0';
}

static void test_parse_value_trims_whitespace(void)
{
    char key[64] = {0};
    char value[128] = {0};

    assert(config_parse_value("  exec = /usr/bin/lumi-shell  \n", key, value, sizeof(value)) == 0);
    assert(strcmp(key, "exec") == 0);
    assert(strcmp(value, "/usr/bin/lumi-shell") == 0);
}

static void test_parse_service_truncates_fields_and_terminates(void)
{
    const char *path = "lumid-test-config.svc";
    char long_name[128];
    char long_desc[512];
    char long_env[512];
    FILE *fp;
    service_t svc;

    memset(&svc, 0, sizeof(svc));
    fill_chars(long_name, sizeof(long_name), 'n');
    fill_chars(long_desc, sizeof(long_desc), 'd');
    fill_chars(long_env, sizeof(long_env), 'v');

    fp = fopen(path, "w");
    assert(fp != NULL);
    fprintf(fp,
            "[service]\n"
            "name = %s\n"
            "description = %s\n"
            "exec = /usr/bin/lumi-shell\n"
            "args = --session mobile\n"
            "user = lumios-user-with-very-long-name\n"
            "group = lumios-group-with-very-long-name\n"
            "workdir = /system/ui/with/a/very/long/path/that/should/still/be/terminated\n"
            "\n"
            "[environment]\n"
            "LONG_ENV = %s\n",
            long_name,
            long_desc,
            long_env);
    fclose(fp);

    assert(config_parse_service(path, &svc) == 0);
    assert(strlen(svc.name) == LUMID_MAX_NAME_LEN - 1);
    assert(svc.name[LUMID_MAX_NAME_LEN - 1] == '\0');
    assert(strlen(svc.description) == LUMID_MAX_PATH_LEN - 1);
    assert(svc.description[LUMID_MAX_PATH_LEN - 1] == '\0');
    assert(strcmp(svc.exec_path, "/usr/bin/lumi-shell") == 0);
    assert(strcmp(svc.exec_args, "--session mobile") == 0);
    assert(svc.env_count == 1);
    assert(svc.env[0].key[LUMID_MAX_NAME_LEN - 1] == '\0');
    assert(svc.env[0].value[LUMID_MAX_PATH_LEN - 1] == '\0');

    remove(path);
}

int main(void)
{
    log_set_level(LOG_DEBUG);

    test_parse_value_trims_whitespace();
    test_parse_service_truncates_fields_and_terminates();

    puts("config tests passed");
    return 0;
}
