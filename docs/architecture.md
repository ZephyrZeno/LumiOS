# LumiOS Public Core Architecture

This document describes the **current architecture boundary of the public LumiOS repository**. It is intentionally narrower than the full private operating system stack.

## 1. Public vs Private Split

### Public LumiOS repository

The public repository focuses on the pieces that are useful to discuss, test, and evolve in the open:

- `lumid` as init / service manager
- the community `lumi-shell` entrypoint
- shared core code such as `lumi-toolkit`, `lumi-render`, and public compositor work
- architecture, performance, security, and build documentation

### Private workspace modules

The local workspace still contains modules that are not mirrored as public repos:

- `LumiShell/` for the full shell / compositor / render / toolkit stack
- `LumiIDE/` for the Lux IDE
- additional integration assets in `LumiApps/`, `TASKS/`, and `TESTS/`

## 2. Repository Graph

```text
LumiPkg       -> package format, install/build tooling
LumiSDK       -> app API, storage bridge, event bus, toolkit
LumiScript    -> compiler, bytecode, runtime, stdlib
LumiApps      -> first-party apps packaged as .lmpk
    ↓
LumiOS public core
    ├── lumid
    ├── community lumi-shell
    └── system docs / public components
    ↓
LumiShell private full UI stack
```

## 3. Boot And Service Control

At the public-core level, the most important boot/runtime contract is still `lumid`.

```text
Bootloader
  ↓
Linux kernel + initramfs
  ↓
lumid (PID 1)
  ├── service dependency ordering
  ├── process supervision
  ├── socket control plane
  └── lumictl command client
```

`lumid` is the service boundary that the public shell and other system-facing tools can integrate with without exposing the full proprietary shell stack.

## 4. Shared Session Model

The current public architecture revolves around a shared session-state contract:

```text
Apps / tools / tests
  ↓
LumiSDK storage + reserved keys
  ↓
storage.db
  ↓
community lumi-shell

lumid
  ↓
system.state.updated / system.state.rejected
  ↓
community lumi-shell daemon bridge
```

### Session data path

- session state is persisted in `storage.db`
- the shell reads that storage as the base snapshot
- CLI flags and environment variables can override or seed shell state
- when storage changes, the shell recomputes its summaries from the merged state

### Event path

- `lumid` can publish `system.state.updated` and `system.state.rejected`
- the shell can attach to the daemon socket and consume those events incrementally
- `event_id` tracking prevents replay of old daemon messages

### Refresh strategy

When `--stay-alive` is enabled, the community shell uses both:

- daemon polling for new events
- storage-directory watching for file changes

This gives the public repo a testable event-driven session loop without depending on the private full UI runtime.

## 5. Community `lumi-shell`

The public `system/core/lumi-shell` should be understood as a **community shell entrypoint**, not the final private graphical shell.

Its job is to model and verify:

- app catalog loading
- session snapshot interpretation
- panel summaries
- daemon/event integration
- keep-alive refresh behavior

This keeps the public contract stable for SDK, app, and service development while the proprietary shell can remain separate.

## 6. Externalized Repositories

Several major areas now have dedicated repositories and should be documented there instead of in `LumiOS`:

| Area | Repository |
|------|------------|
| `.lmpk` tooling and package spec | [LumiPkg](https://github.com/ZephyrZeno/LumiPkg) |
| app SDK and storage/event APIs | [LumiSDK](https://github.com/ZephyrZeno/LumiSDK) |
| language, bytecode, runtime, stdlib | [LumiScript](https://github.com/ZephyrZeno/LumiScript) |

That means this repository no longer owns a first-class copy of the package spec or SDK manual.

## 7. Build/Test Boundaries

### In-repo and public

- `system/core/lumid`: `make`, `make test`
- `system/core/lumi-shell`: `meson compile`, `meson test`

### Out-of-repo or private

- full proprietary shell stack in `LumiShell/`
- package manager internals in `LumiPkg`
- app-facing SDK behavior in `LumiSDK`
- language/runtime behavior in `LumiScript`

## 8. Documentation Map

| Document | Purpose |
|----------|---------|
| [BUILD_AND_TEST.md](BUILD_AND_TEST.md) | current build/test workflow for public components |
| [performance.md](performance.md) | performance goals and implementation notes |
| [security.md](security.md) | security model and hardening strategy |
| [ui-design.md](ui-design.md) | UI direction, including liquid-glass design language |
