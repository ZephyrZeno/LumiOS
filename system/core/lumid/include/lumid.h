/*
 * lumid.h - LumiOS Init System
 *
 * lumid is the PID 1 process of LumiOS, responsible for:
 * lumid 是 LumiOS 的 PID 1 进程，负责:
 *
 * - System init (mount filesystems, cgroups, kernel modules)
 *   系统初始化 (挂载文件系统, cgroups, 内核模块)
 * - Service management (start/stop/restart/monitor)
 *   服务管理 (启动/停止/重启/监控)
 * - Dependency resolution (topological sort between services)
 *   依赖解析 (服务间拓扑排序)
 * - Orphan process reaping
 *   孤儿进程回收
 * - Power management
 *   电源管理
 */

#ifndef LUMID_H
#define LUMID_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/* === Version / 版本 === */
#define LUMID_VERSION_MAJOR  0
#define LUMID_VERSION_MINOR  1
#define LUMID_VERSION_PATCH  0
#define LUMID_VERSION_STRING "0.1.0"

/* === Path constants / 路径常量 === */
#define LUMID_SERVICE_DIR      "/etc/lumid/services"
#define LUMID_RUNTIME_DIR      "/run/lumid"
#define LUMID_SOCKET_PATH      "/run/lumid.sock"
#define LUMID_LOG_DIR          "/var/log/lumid"
#define LUMID_PID_FILE         "/run/lumid.pid"
#define LUMID_CGROUP_ROOT      "/sys/fs/cgroup/lumid"

/* === Limits / 限制 === */
#define LUMID_MAX_SERVICES     128
#define LUMID_MAX_DEPS         32
#define LUMID_MAX_ENV          64
#define LUMID_MAX_NAME_LEN     64
#define LUMID_MAX_PATH_LEN     256
#define LUMID_MAX_LINE_LEN     1024
#define LUMID_MAX_LOG_SIZE     (1024 * 1024)  /* 1MB per service log */

/* === Service state / 服务状态 === */
typedef enum {
    SVC_STATE_STOPPED = 0,
    SVC_STATE_STARTING,
    SVC_STATE_RUNNING,
    SVC_STATE_STOPPING,
    SVC_STATE_FAILED,
    SVC_STATE_RESTARTING,
} svc_state_t;

/* === Restart policy / 重启策略 === */
typedef enum {
    RESTART_NEVER = 0,
    RESTART_ALWAYS,
    RESTART_ON_FAILURE,
    RESTART_ON_ABNORMAL,
} restart_policy_t;

/* === Service type / 服务类型 === */
typedef enum {
    SVC_TYPE_SIMPLE = 0,   /* Run directly, PID = service PID / 直接运行 */
    SVC_TYPE_FORKING,      /* Parent exits after fork / fork 后父进程退出 */
    SVC_TYPE_ONESHOT,      /* Run once then mark done / 运行一次后标记完成 */
    SVC_TYPE_NOTIFY,       /* Notify readiness via sd_notify / 通知就绪 */
} svc_type_t;

/* === Cgroup limits / cgroup 资源限制 === */
typedef struct {
    int64_t  memory_max;    /* Max memory (bytes), -1=unlimited / 最大内存 */
    int64_t  memory_high;   /* Memory high watermark / 内存高水位 */
    int32_t  cpu_weight;    /* CPU weight (1-10000) / CPU 权重 */
    int32_t  cpu_max;       /* CPU max percent (1-100), -1=unlimited */
    int32_t  io_weight;     /* IO weight (1-10000) / IO 权重 */
    int32_t  pids_max;      /* Max PIDs, -1=unlimited / 最大进程数 */
} cgroup_limits_t;

/* === Environment variables / 环境变量 === */
typedef struct {
    char key[LUMID_MAX_NAME_LEN];
    char value[LUMID_MAX_PATH_LEN];
} env_var_t;

/* === Service definition / 服务定义 === */
typedef struct service {
    /* Basic info / 基本信息 */
    char            name[LUMID_MAX_NAME_LEN];
    char            description[LUMID_MAX_PATH_LEN];
    char            exec_path[LUMID_MAX_PATH_LEN];
    char            exec_args[LUMID_MAX_PATH_LEN];
    char            working_dir[LUMID_MAX_PATH_LEN];
    char            config_path[LUMID_MAX_PATH_LEN];
    svc_type_t      type;

    /* User/Group / 用户/组 */
    char            user[LUMID_MAX_NAME_LEN];
    char            group[LUMID_MAX_NAME_LEN];

    /* Dependencies / 依赖关系 */
    char            requires[LUMID_MAX_DEPS][LUMID_MAX_NAME_LEN];
    int             requires_count;
    char            after[LUMID_MAX_DEPS][LUMID_MAX_NAME_LEN];
    int             after_count;
    char            before[LUMID_MAX_DEPS][LUMID_MAX_NAME_LEN];
    int             before_count;

    /* Restart policy / 重启策略 */
    restart_policy_t restart_policy;
    int              restart_delay_ms;
    int              restart_max_retries;
    int              restart_count;

    /* Runtime state / 运行状态 */
    svc_state_t      state;
    pid_t            pid;
    int              exit_code;
    uint64_t         start_time;      /* monotonic ns */
    uint64_t         stop_time;

    /* cgroup */
    cgroup_limits_t  cgroup;

    /* Environment variables / 环境变量 */
    env_var_t        env[LUMID_MAX_ENV];
    int              env_count;

    /* Logging / 日志 */
    int              log_fd;
    char             log_path[LUMID_MAX_PATH_LEN];

    /* Auto-start on boot / 开机自启 */
    bool             enabled;

    /* Linked list pointer / 链表指针 */
    struct service  *next;
} service_t;

/* === IPC commands / IPC 命令 === */
typedef enum {
    CMD_START = 1,
    CMD_STOP,
    CMD_RESTART,
    CMD_STATUS,
    CMD_STATUS_ALL,
    CMD_ENABLE,
    CMD_DISABLE,
    CMD_LOG,
    CMD_POWEROFF,
    CMD_REBOOT,
} ipc_cmd_t;

typedef struct {
    ipc_cmd_t  cmd;
    char       service_name[LUMID_MAX_NAME_LEN];
    int        flags;
} ipc_request_t;

typedef struct {
    int         code;        /* 0 = success / 成功 */
    char        message[LUMID_MAX_PATH_LEN];
    svc_state_t state;
    pid_t       pid;
    int         exit_code;
    uint64_t    uptime;
} ipc_response_t;

/* === Function declarations / 函数声明 === */

/* main.c */
int  lumid_main(int argc, char *argv[]);

/* service.c - Service management / 服务管理 */
service_t *service_create(void);
void       service_free(service_t *svc);
int        service_start(service_t *svc);
int        service_stop(service_t *svc);
int        service_restart(service_t *svc);
const char *service_state_str(svc_state_t state);
service_t *service_find(const char *name);
int        service_load_all(const char *dir);
int        service_resolve_deps(void);
int        service_start_all(void);
void       service_stop_all(void);
int        service_get_count(void);
service_t *service_get_list(void);

/* supervisor.c - Process supervisor / 进程监控 */
int  supervisor_init(void);
void supervisor_run(void);
void supervisor_handle_sigchld(void);
void supervisor_check_services(void);

/* config.c - Config parser / 配置解析 */
int  config_parse_service(const char *path, service_t *svc);
int  config_parse_value(const char *line, char *key, char *value, size_t max);

/* cgroup.c - Cgroup management / cgroup 管理 */
int  cgroup_init(void);
int  cgroup_create(const char *name);
int  cgroup_apply_limits(const char *name, const cgroup_limits_t *limits);
int  cgroup_add_pid(const char *name, pid_t pid);
int  cgroup_remove(const char *name);

/* socket.c - IPC communication / IPC 通信 */
int  socket_server_init(const char *path);
int  socket_server_accept(int server_fd);
int  socket_handle_request(int client_fd);
int  socket_client_connect(const char *path);
int  socket_send_request(int fd, const ipc_request_t *req);
int  socket_recv_response(int fd, ipc_response_t *resp);

/* mount.c - Filesystem mounting / 文件系统挂载 */
int  mount_initial_filesystems(void);
int  mount_fstab(const char *fstab_path);

/* log.c - Logging / 日志系统 */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
} log_level_t;

void log_init(const char *log_dir);
void log_set_level(log_level_t level);
void log_write(log_level_t level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
void log_close(void);

#define LOG_D(fmt, ...) log_write(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) log_write(LOG_INFO,  fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) log_write(LOG_WARN,  fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) log_write(LOG_ERROR, fmt, ##__VA_ARGS__)
#define LOG_F(fmt, ...) log_write(LOG_FATAL, fmt, ##__VA_ARGS__)

/* util.c - Utility functions / 工具函数 */
uint64_t    util_monotonic_ns(void);
const char *util_timestamp(void);
int         util_mkdir_p(const char *path, mode_t mode);
int         util_write_file(const char *path, const char *content);
char       *util_read_file(const char *path);
int         util_parse_size(const char *str, int64_t *out);
int         util_daemonize(void);
int         util_drop_privileges(const char *user, const char *group);

#endif /* LUMID_H */
