# LumiOS

**Public LumiOS core repository for `lumid`, the community `lumi-shell`, core system components, and design docs**

[中文](README.md) | English

The public LumiOS repository now focuses on the parts of the system that can live cleanly in the open:

- `lumid` and `lumictl`
- the community `system/core/lumi-shell`
- public `lumi-toolkit`, `lumi-render`, and `lumi-compositor` code
- system architecture, build, and testing documentation

The full private Shell / compositor / render / toolkit stack lives in the local `LumiShell/` workspace directory and shares the same session-state contract as the public community shell.

## Snapshot Highlights

- `lumid` now includes config and socket unit tests, making the daemon/control boundary much easier to verify.
- The community `lumi-shell` can now:
  - load an app catalog from `guest-manifest.json`
  - read shared session state from `storage.db`
  - optionally consume `system.state.updated` and `system.state.rejected` events from `lumid`
  - stay alive and refresh incrementally through daemon polling plus directory change watching
- Documentation has been rewritten around the current repo split: package management, SDK, and language details now live in `LumiPkg`, `LumiSDK`, and `LumiScript`.

## Repository Scope

```text
LumiOS/
├── system/core/
│   ├── lumid/                init / service manager
│   ├── lumi-shell/           community shell entrypoint
│   ├── lumi-compositor/      public compositor code
│   ├── lumi-render/          render abstraction
│   └── lumi-toolkit/         shared UI utilities
├── docs/                     architecture / build / security / performance
└── README*.md
```

No longer maintained directly in this repository:

- `.lmpk` tooling and package spec → [LumiPkg](https://github.com/ZephyrZeno/LumiPkg)
- application SDK and `liblumiapp` → [LumiSDK](https://github.com/ZephyrZeno/LumiSDK)
- LumiScript language and runtime → [LumiScript](https://github.com/ZephyrZeno/LumiScript)

## Core Components

### `lumid`

`lumid` is the LumiOS init / service manager. It handles:

- service dependency ordering
- basic cgroup and process supervision
- Unix socket based control
- the `lumictl` command-line client

### Community `lumi-shell`

The public `lumi-shell` is not the full graphical shell. It is a portable community entrypoint focused on:

- shared session snapshot reads
- `lumid` event bridging
- panel summary generation
- keep-alive refresh behavior and automated testing

It gives the private full `LumiShell/` stack a public, testable session model without requiring the entire proprietary UI stack to be published.

## Build and Test

### `lumid`

```bash
cd system/core/lumid
make
make test
```

### Community `lumi-shell`

```bash
cd system/core/lumi-shell
meson setup build --prefix=/usr
meson compile -C build
meson test -C build
```

### Manual Shell Summary Run

```bash
./system/core/lumi-shell/build/lumi-shell \
  --panel quick-settings \
  --battery 92 \
  --wifi on \
  --storage-path /tmp/lumi-session.db
```

### Keep-Alive Mode

```bash
./system/core/lumi-shell/build/lumi-shell \
  --stay-alive \
  --storage-path /tmp/lumi-session.db \
  --lumid-socket /tmp/lumid.sock
```

See [docs/BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md) for a fuller walkthrough.

## Documentation

| Document | Description |
|----------|-------------|
| [docs/BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md) | current build, test, and smoke-test guide |
| [docs/architecture.md](docs/architecture.md) | public-core architecture boundaries and shared-state model |
| [docs/performance.md](docs/performance.md) | performance goals and design |
| [docs/security.md](docs/security.md) | security model |
| [docs/ui-design.md](docs/ui-design.md) | UI and liquid-glass direction |
| [system/core/lumi-shell/README.md](system/core/lumi-shell/README.md) | community shell flags and behavior |

## Related Projects

| Project | Description |
|---------|-------------|
| [LumiPkg](https://github.com/ZephyrZeno/LumiPkg) | `.lmpk` package manager and build toolchain |
| [LumiSDK](https://github.com/ZephyrZeno/LumiSDK) | `liblumiapp`, language bindings, and toolkit |
| [LumiScript](https://github.com/ZephyrZeno/LumiScript) | compiler, bytecode, and runtime |

## License

GPLv3 (kernel-related patches remain GPLv2)
