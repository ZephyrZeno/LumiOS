# lumi-shell

`lumi-shell` is the community shell entrypoint for `LumiOS`.

Current responsibilities:

- Load the guest app catalog from `guest-manifest.json`
- Read shared system session state from `storage.db`
- Surface status bar, quick settings, notifications, recents, and lockscreen summaries
- Optionally read `system.state.updated` and `system.state.rejected` events from `lumid`
- While `--stay-alive` is active, poll `lumid` for new events and consume them incrementally
- While `--stay-alive` is active, wait on shared storage directory changes before refreshing state, with a timeout fallback so daemon-driven updates still arrive

## Build

With Meson:

```bash
meson setup build --prefix=/usr
meson compile -C build
meson test -C build
```

With GCC:

```bash
gcc -Wall -Wextra -std=c11 -Iinclude src/main.c src/catalog.c src/daemon.c src/session.c src/state.c src/watch.c -o build/lumi-shell
gcc -Wall -Wextra -std=c11 -Iinclude -Isrc src/daemon.c tests/test_daemon.c -o build/test-daemon-events
gcc -Wall -Wextra -std=c11 -Iinclude -Isrc src/state.c src/daemon.c src/session.c src/watch.c tests/test_runtime.c -o build/test-runtime-refresh
```

## Usage

```bash
lumi-shell --panel quick-settings --storage-path /tmp/lumios.db
```

Common options:

- `--panel <home|launcher|quick-settings|notifications|recents|lockscreen>`
- `--launch <app-id>`
- `--workspace <path>`
- `--status <text>`
- `--recent-action <text>`
- `--storage-path <path>`
- `--lumid-socket <path>`
- `--notify <title|body|source>`
- `--battery <0-100>`
- `--brightness <0-100>`
- `--volume <0-100>`
- `--wifi <on|off>`
- `--bluetooth <on|off>`
- `--mobile-data <on|off>`
- `--dnd <on|off>`
- `--dark-mode <on|off>`
- `--night-light <on|off>`
- `--battery-saver <on|off>`
- `--auto-brightness <on|off>`
- `--locked`
- `--stay-alive`

Environment fallbacks:

- `LUMI_STORAGE_PATH`
- `LUMI_LUMID_SOCKET_PATH`
- `LUMI_WORKSPACE_PATH`
- `LUMI_STATUS_LINE`
- `LUMI_RECENT_ACTION`
- `LUMI_ACTIVE_APP`
- `LUMI_OPEN_PANEL`

Notes:

- On Linux, `lumi-shell` probes `lumid` automatically through `/run/lumid.sock`.
- If the default daemon socket is offline, the shell stays quiet by default.
- If `--lumid-socket` or `LUMI_LUMID_SOCKET_PATH` is set, the session summary will show bridge status explicitly.
- The daemon bridge tracks `event_id` values so repeated polling does not replay old notifications.
- Shared storage reloads start from the base CLI/environment config when the storage signature changes, so removed keys fall back cleanly without forcing a full reparse every poll.
- The idle wait path uses platform file change notifications when available and falls back to a timed sleep if the parent directory cannot be watched.
