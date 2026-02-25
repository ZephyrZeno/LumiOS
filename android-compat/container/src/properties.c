/*
 * properties.c - Android system properties / Android 系统属性
 *
 * Manages Android system properties (build.prop, default.prop)
 * used by the Android runtime inside the container.
 * 管理容器内 Android 运行时使用的系统属性。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "container.h"

#define MAX_PROPS 256
#define MAX_KEY   128
#define MAX_VAL   256

/* Property storage / 属性存储 */
static struct {
    char key[MAX_KEY];
    char value[MAX_VAL];
} g_props[MAX_PROPS];
static int g_prop_count = 0;

/* === Initialize property system / 初始化属性系统 === */

int props_init(void)
{
    g_prop_count = 0;
    memset(g_props, 0, sizeof(g_props));
    return 0;
}

/* === Set a property / 设置属性 === */

int props_set(const char *key, const char *value)
{
    /* Update existing / 更新已有 */
    for (int i = 0; i < g_prop_count; i++) {
        if (strcmp(g_props[i].key, key) == 0) {
            strncpy(g_props[i].value, value, MAX_VAL - 1);
            return 0;
        }
    }

    /* Add new / 添加新属性 */
    if (g_prop_count >= MAX_PROPS) {
        fprintf(stderr, "WARNING: property table full, cannot set '%s'\n", key);
        return -1;
    }

    strncpy(g_props[g_prop_count].key, key, MAX_KEY - 1);
    strncpy(g_props[g_prop_count].value, value, MAX_VAL - 1);
    g_prop_count++;
    return 0;
}

/* === Get a property / 获取属性 === */

const char *props_get(const char *key)
{
    for (int i = 0; i < g_prop_count; i++) {
        if (strcmp(g_props[i].key, key) == 0)
            return g_props[i].value;
    }
    return NULL;
}

/* === Load default Android properties / 加载默认 Android 属性 === */

int props_load_defaults(void)
{
    /* Core Android properties / 核心 Android 属性 */
    props_set("ro.build.display.id", "LumiOS-Android-14");
    props_set("ro.build.version.sdk", "34");
    props_set("ro.build.version.release", "14");
    props_set("ro.build.version.security_patch", "2026-02-01");
    props_set("ro.build.type", "userdebug");
    props_set("ro.build.host", "lumios-build");
    props_set("ro.product.model", "LumiOS Device");
    props_set("ro.product.brand", "LumiOS");
    props_set("ro.product.name", "lumios");
    props_set("ro.product.device", "generic_arm64");
    props_set("ro.product.manufacturer", "LumiOS Project");
    props_set("ro.product.cpu.abilist", "arm64-v8a");
    props_set("ro.product.cpu.abilist64", "arm64-v8a");
    props_set("ro.hardware", "generic_arm64");
    props_set("ro.board.platform", "lumios");
    props_set("ro.debuggable", "1");
    props_set("ro.secure", "0");

    /* Display properties / 显示属性 */
    props_set("ro.sf.lcd_density", "440");
    props_set("persist.sys.timezone", "Asia/Shanghai");
    props_set("persist.sys.language", "zh");
    props_set("persist.sys.country", "CN");

    /* Runtime properties / 运行时属性 */
    props_set("dalvik.vm.heapsize", "512m");
    props_set("dalvik.vm.heapgrowthlimit", "256m");
    props_set("dalvik.vm.heapminfree", "8m");
    props_set("dalvik.vm.heapmaxfree", "32m");

    /* Try loading from properties file / 尝试从属性文件加载 */
    FILE *fp = fopen(ANDROID_PROPS_FILE, "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            /* Skip comments and empty lines / 跳过注释和空行 */
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
                continue;

            char *eq = strchr(line, '=');
            if (!eq) continue;

            *eq = '\0';
            char *val = eq + 1;
            val[strcspn(val, "\n\r")] = '\0';

            /* Trim key / 修剪键 */
            char *key = line;
            while (*key == ' ' || *key == '\t') key++;
            char *ke = key + strlen(key) - 1;
            while (ke > key && (*ke == ' ' || *ke == '\t')) *ke-- = '\0';

            props_set(key, val);
        }
        fclose(fp);
        fprintf(stderr, "[android-container] loaded properties from %s\n",
                ANDROID_PROPS_FILE);
    }

    fprintf(stderr, "[android-container] %d properties loaded\n", g_prop_count);
    return 0;
}
