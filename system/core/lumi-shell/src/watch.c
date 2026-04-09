#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "shell_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include <sys/inotify.h>
#endif

static void copy_string(char *dst, size_t len, const char *src) {
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

static void sleep_fallback(unsigned int timeout_ms) {
#ifdef _WIN32
    Sleep((DWORD)timeout_ms);
#else
    struct timespec delay;

    delay.tv_sec = timeout_ms / 1000U;
    delay.tv_nsec = (long)(timeout_ms % 1000U) * 1000000L;
    nanosleep(&delay, NULL);
#endif
}

static bool extract_parent_directory(const char *path,
                                     char *directory,
                                     size_t directory_len) {
    const char *last_separator;
    size_t used;

    if (!directory || directory_len == 0) {
        return false;
    }

    directory[0] = '\0';
    if (!path || path[0] == '\0') {
        copy_string(directory, directory_len, ".");
        return true;
    }

    last_separator = strrchr(path, '/');
#ifdef _WIN32
    {
        const char *windows_separator = strrchr(path, '\\');
        if (!last_separator || (windows_separator && windows_separator > last_separator)) {
            last_separator = windows_separator;
        }
    }
#endif

    if (!last_separator) {
        copy_string(directory, directory_len, ".");
        return true;
    }

    if (last_separator == path) {
        copy_string(directory, directory_len, "/");
        return true;
    }

    used = (size_t)(last_separator - path);
    if (used >= directory_len) {
        used = directory_len - 1;
    }
    memcpy(directory, path, used);
    directory[used] = '\0';
    return true;
}

bool lumi_shell_wait_for_runtime_signal(const lumi_shell_t *shell, unsigned int timeout_ms) {
    char directory[MAX_PATH_LEN];
    const char *storage_path = NULL;

    if (timeout_ms == 0) {
        return false;
    }

    if (shell) {
        if (shell->config.storage_path && shell->config.storage_path[0] != '\0') {
            storage_path = shell->config.storage_path;
        } else if (shell->shared_state && shell->shared_state->storage_path[0] != '\0') {
            storage_path = shell->shared_state->storage_path;
        }
    }

    if (!extract_parent_directory(storage_path, directory, sizeof(directory))) {
        sleep_fallback(timeout_ms);
        return false;
    }

#ifdef _WIN32
    {
        HANDLE change_handle;
        DWORD wait_result;

        change_handle = FindFirstChangeNotificationA(directory,
                                                     FALSE,
                                                     FILE_NOTIFY_CHANGE_FILE_NAME |
                                                         FILE_NOTIFY_CHANGE_SIZE |
                                                         FILE_NOTIFY_CHANGE_LAST_WRITE);
        if (change_handle == INVALID_HANDLE_VALUE) {
            sleep_fallback(timeout_ms);
            return false;
        }

        wait_result = WaitForSingleObject(change_handle, (DWORD)timeout_ms);
        if (wait_result == WAIT_OBJECT_0) {
            (void)FindNextChangeNotification(change_handle);
            FindCloseChangeNotification(change_handle);
            return true;
        }

        FindCloseChangeNotification(change_handle);
        return false;
    }
#else
    {
        int inotify_fd;
        int watch_descriptor;
        struct pollfd poll_fd;
        char buffer[sizeof(struct inotify_event) * 4];

        inotify_fd = inotify_init1(IN_CLOEXEC);
        if (inotify_fd < 0) {
            sleep_fallback(timeout_ms);
            return false;
        }

        watch_descriptor = inotify_add_watch(inotify_fd,
                                             directory,
                                             IN_ATTRIB | IN_CLOSE_WRITE |
                                                 IN_CREATE | IN_DELETE |
                                                 IN_MOVED_FROM | IN_MOVED_TO);
        if (watch_descriptor < 0) {
            close(inotify_fd);
            sleep_fallback(timeout_ms);
            return false;
        }

        poll_fd.fd = inotify_fd;
        poll_fd.events = POLLIN;
        poll_fd.revents = 0;
        if (poll(&poll_fd, 1, (int)timeout_ms) > 0 && (poll_fd.revents & POLLIN) != 0) {
            (void)read(inotify_fd, buffer, sizeof(buffer));
            close(inotify_fd);
            return true;
        }

        close(inotify_fd);
        return false;
    }
#endif
}
