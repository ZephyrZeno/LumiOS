# LumiOS

**自研移动操作系统 — 基于 Linux 内核，非 AOSP**

[English](README_EN.md) | 中文

LumiOS 是一个从零构建的移动操作系统，使用主线 Linux 内核，拥有自研的用户空间、
包管理系统、Wayland 合成器和移动端 Shell，并通过容器化技术实现 Android APK 兼容。

## 架构概览

```
┌─────────────────────────────────────────────────┐
│                  用户应用层                      │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────┐ │
│  │ 原生 App │ │ .lmpk App│ │ Android APK (容器)│ │
│  └──────────┘ └──────────┘ └──────────────────┘ │
├─────────────────────────────────────────────────┤
│                  Shell & UI                     │
│  ┌─────────────────────────────────────────────┐│
│  │ lumi-shell (主屏/通知/设置/多任务)            ││
│  │ lumi-toolkit (UI 组件库)                     ││
│  └─────────────────────────────────────────────┘│
├─────────────────────────────────────────────────┤
│               图形 & 显示                        │
│  ┌─────────────────────────────────────────────┐│
│  │ lumi-compositor (Wayland wlroots 合成器)     ││
│  │ Mesa/GPU 驱动 (Adreno/Mali/PowerVR)          ││
│  └─────────────────────────────────────────────┘│
├─────────────────────────────────────────────────┤
│              系统服务层                          │
│  ┌────────┐┌────────┐┌────────┐┌──────────────┐ │
│  │ lumid  ││ lmpkg  ││ netd   ││android-compat│ │
│  │ (init) ││(包管理) ││(网络)  ││  (APK 容器)  │ │
│  └────────┘└────────┘└────────┘└──────────────┘ │
├─────────────────────────────────────────────────┤
│              硬件抽象层 (HAL)                    │
│  ┌────────┐┌────────┐┌────────┐┌─────────────┐  │
│  │ 显示   ││ 音频   ││ 相机   ││ 传感器/基带   │  │
│  └────────┘└────────┘└────────┘└─────────────┘  │
├─────────────────────────────────────────────────┤
│              Linux Kernel 6.x (ARM64)           │
│  DRM/KMS │ ALSA │ V4L2 │ Input │ USB │ BT│ Wi-Fi│
└─────────────────────────────────────────────────┘
```

## 核心特性

### 🚀 极致性能
- **零 GC 系统层**: 全 C/C++ 核心，无虚拟机停顿，触摸到显示延迟 < 8ms
- **极简调用栈**: App→toolkit→compositor→kernel (3层 vs Android 6+层)
- **cgroup v2 强制隔离**: 后台自动冻结+限频，空闲内存 < 256MB
- **主动热管理**: 趋势预测而非阈值触发，UI 永驻大核

### 🎨 现代化 UI
- 自研 UI 框架与设计语言
- 暗色/浅色模式

### 🔒 高安全性
- **纵深防御六层**: 硬件信任根 → 验证启动 → 内核加固 → dm-verity → AppArmor → 应用沙箱
- **网络权限默认关闭**: 应用必须显式申请网络访问
- **Landlock + seccomp**: 比 Android SELinux 更细粒度的文件系统和系统调用隔离
- **系统级 tracker blocker**: 内置广告追踪器阻断

### ⚙️ 系统组件
- **自研 Init 系统 (lumid)**: 轻量级 PID 1 进程，服务管理，依赖解析
- **自研包管理 (.lmpk)**: zstd 压缩二进制包，依赖解析，仓库系统
- **Wayland 原生**: 基于 wlroots 的触摸优化合成器
- **APK 兼容**: 通过 Linux 命名空间/容器运行 Android 应用
- **多设备支持**: Xiaomi 14 Pro, Google Pixel, QEMU/VM 虚拟机

## 目录结构

```
LumiOS/
├── system/                 # 系统核心
│   ├── core/               # 核心组件
│   │   ├── lumid/          # Init 系统
│   │   ├── lumi-compositor/# Wayland 合成器
│   │   ├── lumi-shell/     # 移动端 Shell UI
│   │   ├── lumi-render/    # 渲染抽象层
│   │   ├── lumi-toolkit/   # UI 组件库
│   │   └── sandbox/        # 安全沙箱
│   ├── services/           # 系统服务
│   │   ├── netd/           # 网络管理
│   │   ├── audiod/         # 音频服务
│   │   ├── sensord/        # 传感器服务
│   │   ├── telephonyd/     # 电话服务
│   │   └── bluetoothd/     # 蓝牙服务
├── android-compat/         # Android 兼容层
│   └── container/          # LXC/namespace 容器
├── devices/                # 设备适配
│   └── generic-arm64/      # QEMU VM 目标
├── build/                  # 构建系统
│   ├── Makefile            # 顶层构建
│   ├── scripts/            # 构建脚本
│   └── rootfs/             # 根文件系统模板
├── scripts/                # 测试脚本
└── docs/                   # 文档
    ├── architecture.md     # 架构设计
    ├── BUILD_AND_TEST.md   # 编译与测试指南
    ├── performance.md      # 性能优化设计
    ├── security.md         # 安全架构
    └── ui-design.md        # UI 设计规范
```

> **已迁移到独立仓库的组件：**
>
> | 组件 | 独立仓库 | 说明 |
> |------|----------|------|
> | 包管理器 | [LumiPkg](https://github.com/ZephyrZeno/LumiPkg) | CLI 包管理 + .lmpk 构建工具 |
> | 应用开发 SDK | [LumiSDK](https://github.com/ZephyrZeno/LumiSDK) | liblumiapp + 多语言绑定 |
> | LumiScript 语言 | [LumiScript](https://github.com/ZephyrZeno/LumiScript) | 声明式应用开发语言 |

## 技术栈

| 组件 | 技术选型 | 理由 |
|------|----------|------|
| 内核 | Linux 6.12 LTS | 主线支持，ARM64 成熟 |
| C 库 | musl libc | 轻量，静态链接友好 |
| Init | lumid (自研 C) | 最小化，移动优化 |
| 显示 | Wayland + wlroots | 现代，触摸原生 |
| GPU | Mesa (Freedreno/Panfrost) | 开源 GPU 驱动 |
| UI 框架 | lumi-toolkit (C + Skia) | 自研渲染 |
| 包管理 | lmpkg (C + libarchive) | 快速，依赖解析 |
| Android | 容器 (namespaces + binder) | 隔离，兼容 |
| 音频 | PipeWire | 现代，低延迟 |
| 网络 | iwd + networkd | 轻量 Wi-Fi |
| 蓝牙 | BlueZ | 标准 Linux 蓝牙栈 |

## 环境要求

| 依赖 | 最低版本 | 说明 |
|------|---------|------|
| GCC / Clang | GCC 11+ / Clang 14+ | C11/C17 编译器 |
| Meson | 0.60+ | 构建系统 (compositor/shell) |
| Ninja | 1.10+ | 构建后端 |
| pkg-config | — | 依赖查找 |
| Wayland | 1.21+ | 显示协议 |
| wlroots | 0.17+ | Wayland 合成器库 |
| Cairo | 1.16+ | 2D 渲染 (软件后端) |

> **注意**: LumiOS 核心系统组件（合成器、Shell）需要 Linux 环境构建和运行。
> Windows 仅支持构建 lumid (init)、netd 等纯 C 组件的交叉编译。

## 环境安装

### Ubuntu 24.04 / 22.04 (推荐)

```bash
# 基础构建工具
sudo apt update
sudo apt install -y build-essential meson ninja-build pkg-config

# Wayland + 图形依赖
sudo apt install -y \
    libwayland-dev wayland-protocols libwlroots-dev libxkbcommon-dev \
    libinput-dev libpixman-1-dev libegl-dev libgles2-mesa-dev \
    libdrm-dev libgbm-dev libcairo2-dev libpango1.0-dev \
    libglib2.0-dev libgdk-pixbuf-2.0-dev

# 可选: 交叉编译 (aarch64 目标)
sudo apt install -y gcc-aarch64-linux-gnu

# 可选: VM 测试
sudo apt install -y qemu-system-aarch64 qemu-utils
```

验证:
```bash
gcc --version       # gcc 11+
meson --version     # 0.60+
pkg-config --version
```

### Windows (仅限纯 C 组件)

LumiOS 的 Wayland 合成器和 Shell 需要 Linux 环境。Windows 上仅可编译不依赖 Wayland 的组件（lumid、android-compat 等）。

1. **下载 MinGW-w64 GCC 13.2.0**:
   - [WinLibs GCC 13.2.0 (x86_64, UCRT)](https://github.com/brechtsanders/winlibs_mingw/releases/download/13.2.0posix-18.1.5-11.0.1-ucrt-r5/winlibs-x86_64-posix-seh-gcc-13.2.0-mingw-w64ucrt-11.0.1-r5.7z)

2. **解压到** `C:\mingw64`，添加 `C:\mingw64\bin` 到 PATH

3. **推荐使用 WSL2**: 安装 Ubuntu 22.04/24.04 以获得完整构建能力
   ```powershell
   wsl --install -d Ubuntu-24.04
   ```

## 快速开始

详细编译与测试指南请参考 [docs/BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md)

### 编译 & 测试 (Ubuntu)

```bash
# 编译合成器
cd system/core/lumi-compositor
meson setup build --prefix=/usr
ninja -C build

# 编译 Shell
cd system/core/lumi-shell
meson setup build --prefix=/usr
ninja -C build

# 横屏测试
WLR_BACKENDS=x11 WLR_RENDERER=pixman LIBGL_ALWAYS_SOFTWARE=1 \
    system/core/lumi-compositor/build/lumi-compositor -s wayland-lumi &
sleep 2 && WAYLAND_DISPLAY=wayland-lumi system/core/lumi-shell/build/lumi-shell

# 竖屏测试 (450x900)
chmod +x scripts/test-portrait.sh
./scripts/test-portrait.sh 450x900
```

### 纯 C 组件编译 (Windows / Ubuntu 均可)

```bash
# lumid (init 系统)
cd system/core/lumid
gcc -Wall -O2 -std=c11 -o lumid src/*.c

# android-compat (容器)
cd android-compat/container
gcc -Wall -O2 -std=c11 -o android-container src/*.c
```

### 完整系统构建 (Ubuntu)

```bash
make all DEVICE=generic-arm64    # 完整构建
make run-vm                       # QEMU 运行
```

## 设计文档

| 文档 | 内容 |
|---|---|
| [docs/architecture.md](docs/architecture.md) | 系统总体架构 |
| [docs/performance.md](docs/performance.md) | 性能优化设计 (安卓痛点解决方案) |
| [docs/ui-design.md](docs/ui-design.md) | UI 设计规范 |
| [docs/security.md](docs/security.md) | 安全架构 (纵深防御/沙箱/加密) |
| [docs/BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md) | 编译与测试指南 (Ubuntu 24.04) |

> **已迁移到独立仓库的文档：**
> - .lmpk 包格式规范 → LumiPkg (`docs/lmpk-spec.md`)
> - 应用开发指南 → LumiSDK
> - LumiScript 语言规范 → LumiScript

## 开发状态

### ✅ 已完成

- [x] 项目架构设计 + 目录结构
- [x] 性能优化/安全加固/UI 设计规范文档 (5 份)
- [x] 构建系统 (Makefile + test-build.sh + build-vm.sh)
- [x] **Init 系统 (lumid)** — 10 个源文件，x86_64 + aarch64 编译通过，功能测试通过
  - 服务配置加载 (.svc INI 格式)
  - 依赖拓扑排序 + 按序启动
  - 进程监控 + 自动重启策略
  - cgroup v2 资源隔离
  - IPC 套接字 + lumictl 控制工具
  - 优雅关闭信号处理
- [x] **包管理器 (lmpkg)** — 已迁移至 [LumiPkg](https://github.com/ZephyrZeno/LumiPkg) 独立仓库
- [x] **Wayland 合成器 (lumi-compositor)** — 基于 wlroots，触摸优化
- [x] **移动端 Shell (lumi-shell)** — 主屏启动器/锁屏/通知中心/多任务/状态栏/开机动画
- [x] **Android 兼容层 (container)** — 6 个源文件，x86_64 + aarch64 编译通过
  - Linux namespace 容器隔离
  - cgroup 资源限制
  - Binder 代理 + 属性系统
- [x] **系统服务 — netd (网络管理)** — 7 个源文件，编译通过
  - Wi-Fi (iwd/wpa_supplicant)，DHCP，DNS，网络监控
- [x] **系统服务 — audiod (音频)** — 5 个源文件
  - PipeWire 集成，每流音量控制，持久化配置
- [x] **渲染抽象层 (lumi-render)** — Cairo/Skia 双后端
- [x] **UI 控件工具包 (lumi-toolkit)** — 布局引擎 + 控件渲染 + 事件分发
- [x] **第一方应用** — 浏览器/相机/文件管理器/终端/设置/电话/消息
- [x] **设备适配 (QEMU/VM)** — 内核配置 + rootfs + 镜像脚本
- [x] **lumid 服务配置 (.svc)** — devd/dbusd/audiod/bootanim/netd/compositor/shell
- [x] **Linux 6.1.73 LTS aarch64 内核编译** — QEMU virt 平台
- [x] **initramfs + rootfs + 磁盘镜像生成**

### 🔨 构建验证

| 组件 | x86_64 编译 | aarch64 交叉编译 | 功能测试 |
|---|---|---|---|
| lumid | ✅ PASS | ✅ PASS (932K) | ✅ --test 全功能 |
| lmpkg | ✅ PASS | ⚠️ 需交叉 libarchive | ✅ 11 单元测试 |
| android-container | ✅ PASS | ✅ PASS (732K) | — |
| netd | ✅ PASS | ✅ PASS (656K) | — |
| lumi-compositor | ✅ PASS | — | ✅ 竖屏/横屏测试 |
| lumi-shell | ✅ PASS (14 src) | — | ✅ VM 竖屏测试 |
| lumi-render | ✅ PASS (6 src) | — | ✅ Cairo + Skia vtable |
| lumi-toolkit | ✅ PASS (5 src) | — | ✅ 布局+渲染+事件 |
| sandbox | ✅ PASS (5 src) | — | ✅ CLI 工具 |
| bluetoothd | ✅ PASS (5 src) | — | — |
| sensord | ✅ PASS (5 src) | — | — |
| telephonyd | ✅ PASS (8 src) | — | — |

### 📋 待开发

- [x] lumi-render (渲染抽象层，Cairo/Skia 双后端)
- [x] lumi-toolkit (Flexbox 布局 + 控件渲染 + 事件分发)
- [x] Skia GPU 后端集成 (skia_c.h/cpp + backend_skia.c + Meson)
- [x] 开机动画 (Logo 发光 + 真实 dmesg 日志)
- [x] 设置个性化分类 (壁纸/锁屏/桌面/图标/字体)
- [x] 壁纸动态适配 (任意图片自动裁切到屏幕尺寸)
- [x] 竖屏模式支持 (450x900 测试通过)
- [x] 安全沙箱框架 (Landlock + seccomp + AppArmor) — 5 个源文件
- [x] 系统服务: bluetoothd (5 src) / sensord (5 src) / telephonyd (8 src)
- [x] 内置应用
- [x] 应用开发 SDK → 已迁移至 [LumiSDK](https://github.com/ZephyrZeno/LumiSDK)
- [x] LumiScript 语言 → 已迁移至 [LumiScript](https://github.com/ZephyrZeno/LumiScript)
- [ ] 设备适配 (Google Pixel)
- [ ] 设备适配 (Xiaomi 14 Pro)
- [ ] OTA 更新系统
- [ ] 完整 VM boot 测试 (Ubuntu + QEMU)

## 🚀 未来目标：双模式系统

LumiOS 将支持**移动模式**和 **PC 模式**双形态切换，连接外接显示器/键鼠时自动进入 PC 模式。

- **移动模式**: 全屏应用栈 + 手势导航 + 触摸优先
- **PC 模式**: 多窗口管理 + 任务栏 + 键鼠优先

软件兼容目标：lmpkg 原生应用、APK 安卓应用、Linux 包 (deb/Flatpak)、Windows 应用 (Wine)

## 🧠 未来目标：AI 原生集成

LumiOS 将在系统层原生集成大语言模型（LLM），实现屏幕感知 + 智能操作。

- **屏幕内容识别** — OCR + 多模态视觉模型理解屏幕内容
- **用户意图预测** — 分析操作习惯，主动建议下一步操作
- **自动化操作** — AI 代替用户完成复杂交互流程
- **本地推理优先** — 端侧小模型处理隐私敏感任务，复杂任务可选云端

应用方向：智能助手、自动化任务、智能家居、文档处理、无障碍辅助等

## 许可证

GPLv3 (内核部分遵循 GPLv2)
