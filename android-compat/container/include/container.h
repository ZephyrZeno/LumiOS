/*
 * container.h - Android Compatibility Container / Android 兼容容器
 *
 * Manages the containerized Android runtime using Linux namespaces.
 * Provides isolated environment for running APK applications.
 *
 * 使用 Linux 命名空间管理容器化的 Android 运行时。
 * 为运行 APK 应用提供隔离环境。
 */

#ifndef CONTAINER_H
#define CONTAINER_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/* === Version / 版本 === */
#define CONTAINER_VERSION "0.1.0"

/* === Path constants / 路径常量 === */
#define ANDROID_ROOT       "/var/lib/android"
#define ANDROID_DATA       "/var/lib/android/data"
#define ANDROID_SYSTEM     "/var/lib/android/system"
#define ANDROID_VENDOR     "/var/lib/android/vendor"
#define ANDROID_RUNTIME    "/var/lib/android/runtime"
#define ANDROID_PROPS_FILE "/var/lib/android/default.prop"
#define CONTAINER_PID_FILE "/run/android-container.pid"
#define CONTAINER_LOG      "/var/log/android-container.log"

/* === Namespace flags / 命名空间标志 === */
#define NS_MOUNT   (1 << 0)
#define NS_PID     (1 << 1)
#define NS_NET     (1 << 2)
#define NS_IPC     (1 << 3)
#define NS_UTS     (1 << 4)
#define NS_USER    (1 << 5)
#define NS_CGROUP  (1 << 6)
#define NS_ALL     (NS_MOUNT | NS_PID | NS_NET | NS_IPC | NS_UTS | NS_CGROUP)

/* === Container state / 容器状态 === */
typedef enum {
    CT_STOPPED = 0,
    CT_STARTING,
    CT_RUNNING,
    CT_STOPPING,
    CT_FAILED,
} ct_state_t;

/* === Container config / 容器配置 === */
typedef struct {
    uint32_t ns_flags;         /* Which namespaces to create / 创建哪些命名空间 */
    bool     share_network;    /* Share host network ns / 共享宿主网络命名空间 */
    bool     gpu_passthrough;  /* Enable GPU access / 启用 GPU 访问 */
    bool     binder_enabled;   /* Enable binder driver / 启用 binder 驱动 */
    int64_t  memory_limit;     /* Memory limit bytes, -1=unlimited / 内存限制 */
    int32_t  cpu_shares;       /* CPU shares / CPU 份额 */
    char     hostname[64];     /* Container hostname / 容器主机名 */
} ct_config_t;

/* === Container instance / 容器实例 === */
typedef struct {
    ct_state_t state;
    ct_config_t config;
    pid_t      init_pid;       /* PID of container init / 容器 init PID */
    int        status_pipe[2]; /* Status communication / 状态通信管道 */
} container_t;

/* === Function declarations / 函数声明 === */

/* container.c - Lifecycle management / 生命周期管理 */
int  container_init(container_t *ct, const ct_config_t *config);
int  container_start(container_t *ct);
int  container_stop(container_t *ct);
int  container_wait(container_t *ct);
void container_destroy(container_t *ct);
const char *container_state_str(ct_state_t state);

/* namespace.c - Namespace setup / 命名空间设置 */
int  ns_create(uint32_t flags);
int  ns_setup_mount(void);
int  ns_setup_pid(void);
int  ns_setup_network(bool share_host);
int  ns_setup_uts(const char *hostname);
int  ns_setup_cgroup(const char *name, int64_t mem_limit, int32_t cpu_shares);

/* mounts.c - Filesystem mounts for container / 容器文件系统挂载 */
int  mounts_setup_android_rootfs(void);
int  mounts_bind_device_nodes(void);
int  mounts_setup_shared_storage(void);
int  mounts_setup_gpu_access(void);

/* cgroup.c - Container resource limits / 容器资源限制 */
int  ct_cgroup_create(const char *name);
int  ct_cgroup_set_limits(const char *name, int64_t mem, int32_t cpu);
int  ct_cgroup_add_pid(const char *name, pid_t pid);
int  ct_cgroup_destroy(const char *name);

/* properties.c - Android system properties / Android 系统属性 */
int  props_init(void);
int  props_set(const char *key, const char *value);
const char *props_get(const char *key);
int  props_load_defaults(void);

#endif /* CONTAINER_H */
