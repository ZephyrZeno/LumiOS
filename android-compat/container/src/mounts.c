/*
 * mounts.c - Container filesystem mounts / 容器文件系统挂载
 *
 * Sets up the Android rootfs, device nodes, shared storage,
 * and GPU access inside the container.
 * 在容器内设置 Android 根文件系统、设备节点、共享存储和 GPU 访问。
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "container.h"

/* Helper: mkdir + bind mount / 辅助: 创建目录 + 绑定挂载 */
static int bind_mount(const char *src, const char *dst, bool readonly)
{
    struct stat st;
    if (stat(src, &st) < 0) return -1;

    mkdir(dst, 0755);
    if (mount(src, dst, NULL, MS_BIND, NULL) < 0) {
        fprintf(stderr, "WARNING: bind mount %s -> %s failed: %s\n",
                src, dst, strerror(errno));
        return -1;
    }

    if (readonly) {
        mount(NULL, dst, NULL, MS_BIND | MS_REMOUNT | MS_RDONLY, NULL);
    }
    return 0;
}

/* === Setup Android rootfs / 设置 Android 根文件系统 === */

int mounts_setup_android_rootfs(void)
{
    fprintf(stderr, "[android-container] setting up Android rootfs\n");

    /*
     * Expected layout at ANDROID_ROOT:
     * 预期 ANDROID_ROOT 的布局:
     *   /var/lib/android/system/   - Android system image (read-only)
     *   /var/lib/android/vendor/   - Vendor image (read-only)
     *   /var/lib/android/data/     - App data (read-write)
     */

    /* Mount tmpfs for Android /dev / 为 Android /dev 挂载 tmpfs */
    char dev_path[256];
    snprintf(dev_path, sizeof(dev_path), "%s/dev", ANDROID_ROOT);
    mkdir(dev_path, 0755);
    mount("tmpfs", dev_path, "tmpfs", MS_NOSUID, "mode=0755,size=64k");

    /* Mount proc / 挂载 proc */
    char proc_path[256];
    snprintf(proc_path, sizeof(proc_path), "%s/proc", ANDROID_ROOT);
    mkdir(proc_path, 0555);
    mount("proc", proc_path, "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL);

    /* Mount sysfs (limited) / 挂载 sysfs（受限） */
    char sys_path[256];
    snprintf(sys_path, sizeof(sys_path), "%s/sys", ANDROID_ROOT);
    mkdir(sys_path, 0555);
    mount("sysfs", sys_path, "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV | MS_RDONLY, NULL);

    /* Mount tmpfs for /tmp and /run / 为 /tmp 和 /run 挂载 tmpfs */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s/tmp", ANDROID_ROOT);
    mkdir(tmp_path, 01777);
    mount("tmpfs", tmp_path, "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777");

    fprintf(stderr, "[android-container] rootfs setup complete\n");
    return 0;
}

/* === Bind essential device nodes / 绑定必要的设备节点 === */

int mounts_bind_device_nodes(void)
{
    fprintf(stderr, "[android-container] binding device nodes\n");

    char dev_base[256];
    snprintf(dev_base, sizeof(dev_base), "%s/dev", ANDROID_ROOT);

    /* Essential devices / 必要设备 */
    struct { const char *name; int major; int minor; mode_t mode; } devs[] = {
        { "null",    1, 3, S_IFCHR | 0666 },
        { "zero",    1, 5, S_IFCHR | 0666 },
        { "random",  1, 8, S_IFCHR | 0666 },
        { "urandom", 1, 9, S_IFCHR | 0666 },
        { NULL, 0, 0, 0 }
    };

    for (int i = 0; devs[i].name; i++) {
        char path[384];
        snprintf(path, sizeof(path), "%s/%s", dev_base, devs[i].name);
        if (mknod(path, devs[i].mode, makedev(devs[i].major, devs[i].minor)) < 0) {
            if (errno != EEXIST) {
                fprintf(stderr, "WARNING: mknod %s failed: %s\n",
                        path, strerror(errno));
            }
        }
    }

    /* Bind mount binder device if available / 如可用则绑定挂载 binder 设备 */
    struct stat st;
    if (stat("/dev/binder", &st) == 0) {
        char binder_path[384];
        snprintf(binder_path, sizeof(binder_path), "%s/binder", dev_base);
        bind_mount("/dev/binder", binder_path, false);
        fprintf(stderr, "[android-container] binder device bound\n");
    }

    return 0;
}

/* === Setup shared storage between host and container / 设置宿主与容器共享存储 === */

int mounts_setup_shared_storage(void)
{
    /*
     * Bind mount /data/shared -> /sdcard inside container.
     * 绑定挂载 /data/shared -> 容器内的 /sdcard。
     */
    char sdcard_path[256];
    snprintf(sdcard_path, sizeof(sdcard_path), "%s/sdcard", ANDROID_ROOT);
    mkdir(sdcard_path, 0770);

    const char *shared_dir = "/data/shared";
    struct stat st;
    if (stat(shared_dir, &st) == 0) {
        bind_mount(shared_dir, sdcard_path, false);
        fprintf(stderr, "[android-container] shared storage bound: %s -> /sdcard\n",
                shared_dir);
    }

    return 0;
}

/* === Setup GPU passthrough / 设置 GPU 直通 === */

int mounts_setup_gpu_access(void)
{
    char dev_base[256];
    snprintf(dev_base, sizeof(dev_base), "%s/dev", ANDROID_ROOT);

    /*
     * Bind mount GPU device nodes for hardware acceleration.
     * 绑定挂载 GPU 设备节点以启用硬件加速。
     *
     * Adreno: /dev/kgsl-3d0
     * Mali: /dev/mali0
     * Generic DRM: /dev/dri/renderD128
     */
    const char *gpu_devs[] = {
        "/dev/dri/renderD128",
        "/dev/dri/card0",
        "/dev/kgsl-3d0",
        "/dev/mali0",
        NULL,
    };

    /* Create /dev/dri in container / 在容器中创建 /dev/dri */
    char dri_path[384];
    snprintf(dri_path, sizeof(dri_path), "%s/dri", dev_base);
    mkdir(dri_path, 0755);

    int bound = 0;
    for (int i = 0; gpu_devs[i]; i++) {
        struct stat st;
        if (stat(gpu_devs[i], &st) == 0) {
            char dst[512];
            snprintf(dst, sizeof(dst), "%s%s", dev_base,
                     gpu_devs[i] + 4); /* strip /dev prefix / 去掉 /dev 前缀 */

            /* Ensure parent dir exists / 确保父目录存在 */
            char *slash = strrchr(dst, '/');
            if (slash) { *slash = '\0'; mkdir(dst, 0755); *slash = '/'; }

            if (bind_mount(gpu_devs[i], dst, false) == 0) {
                fprintf(stderr, "[android-container] GPU device bound: %s\n",
                        gpu_devs[i]);
                bound++;
            }
        }
    }

    if (bound == 0) {
        fprintf(stderr, "WARNING: no GPU devices found for passthrough\n");
    }

    return 0;
}
