# LumiOS

**A mobile operating system built from scratch on the Linux kernel**

[中文](README.md) | English

LumiOS is a from-scratch mobile operating system using the mainline Linux kernel, with a custom userspace, Wayland compositor, mobile shell, and Android APK compatibility via containerization.

## Architecture

```
┌─────────────────────────────────────────────────┐
│                 Applications                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────┐ │
│  │Native App│ │ .lmpk App│ │ Android APK (CTR)│ │
│  └──────────┘ └──────────┘ └──────────────────┘ │
├─────────────────────────────────────────────────┤
│                  Shell & UI                      │
│  ┌─────────────────────────────────────────────┐ │
│  │ lumi-shell (home/notifications/multitask)   │ │
│  │ lumi-toolkit (UI widget library)            │ │
│  └─────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────┤
│               Graphics & Display                 │
│  ┌─────────────────────────────────────────────┐ │
│  │ lumi-compositor (Wayland/wlroots)           │ │
│  │ Mesa/GPU drivers (Adreno/Mali/PowerVR)      │ │
│  └─────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────┤
│              System Services                     │
│  ┌────────┐┌────────┐┌────────┐┌──────────────┐ │
│  │ lumid  ││ netd   ││ audiod ││android-compat│ │
│  │ (init) ││(network)││(audio) ││  (APK CTR)   │ │
│  └────────┘└────────┘└────────┘└──────────────┘ │
├─────────────────────────────────────────────────┤
│                    HAL                           │
│  Display │ Audio │ Camera │ Sensors │ Modem      │
├─────────────────────────────────────────────────┤
│              Linux Kernel 6.x (ARM64)            │
│  DRM/KMS │ ALSA │ V4L2 │ Input │ USB │ BT│ Wi-Fi│
└─────────────────────────────────────────────────┘
```

## Key Features

- **Zero-GC system layer** — Pure C/C++ core, no VM pauses, touch-to-display < 8ms
- **Custom init (lumid)** — Lightweight PID 1, service management, dependency resolution
- **Wayland native** — wlroots-based compositor optimized for touch
- **Android compatibility** — Run APKs via Linux namespace containers
- **Deep security** — Verified boot, dm-verity, AppArmor, Landlock + seccomp sandboxing

## Directory Structure

```
LumiOS/
├── system/core/            Core components
│   ├── lumid/              Init system
│   ├── lumi-compositor/    Wayland compositor (Community Edition)
│   ├── lumi-shell/         Mobile shell (Community Edition)
│   ├── lumi-render/        Render abstraction (Community Edition)
│   ├── lumi-toolkit/       UI toolkit (Community Edition)
│   └── sandbox/            Security sandbox
├── system/services/        System services
│   ├── netd/               Network management
│   ├── audiod/             Audio (PipeWire)
│   ├── sensord/            Sensors
│   ├── telephonyd/         Telephony
│   └── bluetoothd/         Bluetooth
├── android-compat/         Android compatibility layer
├── devices/                Device configs (QEMU, Pixel, Xiaomi)
├── build/                  Build system
├── scripts/                Test scripts
└── docs/                   Documentation
```


## Requirements

| Dependency | Min Version | Notes |
|-----------|-------------|-------|
| GCC / Clang | GCC 11+ / Clang 14+ | C11 compiler |
| Meson | 0.60+ | Build system |
| Ninja | 1.10+ | Build backend |
| Wayland | 1.21+ | Display protocol |
| wlroots | 0.17+ | Compositor library |
| Cairo | 1.16+ | 2D rendering |

> Compositor and Shell require Linux. Windows can only build pure-C components (lumid, netd, etc.).

## Setup

### Ubuntu 24.04 / 22.04 (Recommended)

```bash
sudo apt update
sudo apt install -y build-essential meson ninja-build pkg-config \
    libwayland-dev wayland-protocols libwlroots-dev libxkbcommon-dev \
    libinput-dev libpixman-1-dev libegl-dev libgles2-mesa-dev \
    libdrm-dev libgbm-dev libcairo2-dev libpango1.0-dev \
    libglib2.0-dev libgdk-pixbuf-2.0-dev

# Optional: cross-compile for aarch64
sudo apt install -y gcc-aarch64-linux-gnu

# Optional: VM testing
sudo apt install -y qemu-system-aarch64 qemu-utils
```

### Windows (pure-C components only)

1. Download [MinGW-w64 GCC 13.2.0](https://github.com/brechtsanders/winlibs_mingw/releases/download/13.2.0posix-18.1.5-11.0.1-ucrt-r5/winlibs-x86_64-posix-seh-gcc-13.2.0-mingw-w64ucrt-11.0.1-r5.7z)
2. Extract to `C:\mingw64`, add `C:\mingw64\bin` to PATH
3. **Recommended:** Use WSL2 with Ubuntu for full build support

## Build

```bash
# Full build (Ubuntu)
./build.sh

# Individual components
./build.sh compositor
./build.sh lumid

# Pure-C components (Windows/Ubuntu)
cd system/core/lumid && gcc -Wall -O2 -std=c11 -o lumid src/*.c
```

## Development Status

- [x] System architecture + directory structure
- [x] Init system (lumid) — service config, dependency sort, cgroup v2, process monitor
- [x] Package manager (lmpkg) → [LumiPkg](https://github.com/ZephyrZeno/LumiPkg)
- [x] Wayland compositor (lumi-compositor) — wlroots, touch-optimized
- [x] Mobile shell (lumi-shell) — home/lockscreen/notifications/multitask/statusbar/boot animation
- [x] Render abstraction (lumi-render) — Cairo/Skia dual backend
- [x] UI toolkit (lumi-toolkit) — layout engine + widget rendering + event dispatch
- [x] System services — netd, audiod, bluetoothd, sensord, telephonyd
- [x] Security sandbox — Landlock + seccomp + AppArmor
- [x] Android compatibility layer — namespace containers + binder proxy
- [x] Built-in apps — Browser/Camera/Files/Terminal/Settings/Phone/Messages
- [x] LumiScript bytecode VM — compiler + stack VM + GC + C FFI
- [x] LumiScript apps — 7 system apps rewritten from C to .ls, compiled to .lmpk bytecode
- [x] Shell script app loading — lumi-vm-ffi dynamic loading + UI tree rendering + event firing
- [x] UI modernization — rounded corners/gradients/shadows/frosted glass/iOS-style Switch
- [ ] Shell event system end-to-end verification
- [ ] Device adaptation (Google Pixel / Xiaomi 14 Pro)
- [ ] OTA update system
- [ ] Full VM boot test (Ubuntu + QEMU)

## Related Projects

| Project | Description |
|---------|------------|
| [LumiSDK](https://github.com/ZephyrZeno/LumiSDK) | Application development SDK |
| [LumiPkg](https://github.com/ZephyrZeno/LumiPkg) | Package manager (lmpkg) |
| [LumiScript](https://github.com/ZephyrZeno/LumiScript) | Programming language |

## License

GPLv3 (kernel patches: GPLv2)
