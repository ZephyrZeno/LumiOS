# LumiOS Build And Test Guide

This guide documents the **current public-repo workflow** for LumiOS. It focuses on the components that actually live in this repository today:

- `system/core/lumid`
- `system/core/lumi-shell` (community edition)

The full private shell stack in `LumiShell/` is intentionally out of scope here.

## 1. Recommended Environment

### Linux / WSL2

Recommended for all build and test work.

| Tool | Minimum |
|------|---------|
| GCC / Clang | GCC 11+ / Clang 14+ |
| GNU Make | 4.0+ |
| Meson | 0.60+ |
| Ninja | 1.10+ |

Ubuntu / Debian:

```bash
sudo apt update
sudo apt install -y build-essential meson ninja-build pkg-config
```

### Windows

You can edit the source tree on Windows, but Linux or WSL2 is still the recommended execution environment.

- `lumid` sources are portable C
- the public `lumi-shell` is much lighter than the private full shell stack
- the current `Makefile` / `meson test` workflow is still easiest to run under Linux or WSL2

## 2. Build `lumid`

```bash
cd system/core/lumid
make
```

Outputs:

- `build/lumid`
- `build/lumictl`

## 3. Test `lumid`

```bash
cd system/core/lumid
make test
```

Current test coverage includes:

- config parsing
- socket protocol behavior
- service test hooks when the local test file exists

## 4. Build Community `lumi-shell`

```bash
cd system/core/lumi-shell
meson setup build --prefix=/usr
meson compile -C build
```

Output:

- `build/lumi-shell`

## 5. Test Community `lumi-shell`

```bash
cd system/core/lumi-shell
meson test -C build
```

Current Meson tests:

- `runtime-refresh`
- `daemon-events` on non-Windows hosts

## 6. Smoke Test `lumi-shell`

The public shell can be exercised as a session-summary executable without needing the full private compositor stack.

### One-shot summary

```bash
./system/core/lumi-shell/build/lumi-shell \
  --panel quick-settings \
  --battery 92 \
  --brightness 60 \
  --wifi on \
  --bluetooth off \
  --storage-path /tmp/lumi-session.db
```

### Keep-alive mode with daemon bridge

```bash
./system/core/lumi-shell/build/lumi-shell \
  --stay-alive \
  --panel home \
  --storage-path /tmp/lumi-session.db \
  --lumid-socket /tmp/lumid.sock
```

Behavior in keep-alive mode:

- reads shared session state from `storage.db`
- optionally reads `system.state.updated` / `system.state.rejected` events from `lumid`
- tracks `event_id` values to avoid replaying old daemon events
- watches the storage directory for changes and falls back to timed sleeps if file watching is unavailable

## 7. Common CLI Flags

### `lumi-shell`

```text
--panel <home|launcher|quick-settings|notifications|recents|lockscreen>
--launch <app-id>
--workspace <path>
--status <text>
--recent-action <text>
--storage-path <path>
--lumid-socket <path>
--notify <title|body|source>
--battery <0-100>
--brightness <0-100>
--volume <0-100>
--wifi <on|off>
--bluetooth <on|off>
--mobile-data <on|off>
--dnd <on|off>
--dark-mode <on|off>
--night-light <on|off>
--battery-saver <on|off>
--auto-brightness <on|off>
--locked
--stay-alive
```

### Environment fallbacks

```text
LUMI_STORAGE_PATH
LUMI_LUMID_SOCKET_PATH
LUMI_WORKSPACE_PATH
LUMI_STATUS_LINE
LUMI_RECENT_ACTION
LUMI_ACTIVE_APP
LUMI_OPEN_PANEL
```

## 8. Manual GCC Fallback

### `lumid`

```bash
cd system/core/lumid
gcc -O2 -Wall -Wextra -Wpedantic -std=c11 -Iinclude \
  src/main.c src/service.c src/supervisor.c src/config.c src/cgroup.c \
  src/socket.c src/mount.c src/log.c src/util.c \
  -o build/lumid
```

### `lumi-shell`

```bash
cd system/core/lumi-shell
gcc -Wall -Wextra -std=c11 -Iinclude \
  src/main.c src/catalog.c src/daemon.c src/session.c src/state.c src/watch.c \
  -o build/lumi-shell
```

## 9. Repo Boundaries

- Package management docs and `.lmpk` spec now live in `LumiPkg`
- SDK docs now live in `LumiSDK`
- LumiScript language/runtime docs now live in `LumiScript`
- The private full shell stack belongs to the local `LumiShell/` workspace directory rather than this public repo
