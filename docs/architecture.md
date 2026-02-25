# LumiOS 系统架构设计

## 1. 设计原则

1. **最小化**: 每个组件只做一件事，做好
2. **安全隔离**: Android 兼容层完全容器化，不污染原生系统
3. **触摸优先**: 所有 UI 交互针对触摸屏优化，同时支持键鼠
4. **快速启动**: 冷启动目标 < 3 秒到锁屏
5. **低内存**: 基础系统内存占用 < 256MB

## 2. 启动流程

```
┌─────────────┐
│  Bootloader  │  (设备厂商: U-Boot / ABL)
│  加载 boot.img│
└──────┬──────┘
       │
┌──────▼──────┐
│ Linux Kernel │  ARM64, 设备树, initramfs
│  6.12 LTS    │
└──────┬──────┘
       │
┌──────▼──────┐
│   initramfs  │  最小 rootfs, 挂载真实根分区
│  /init 脚本  │
└──────┬──────┘
       │
┌──────▼──────┐
│    lumid     │  PID 1, 服务管理器
│  (系统初始化) │
└──────┬──────┘
       │ 按依赖顺序启动服务
       ├──► devd (设备管理, udev 规则)
       ├──► dbusd (系统总线)
       ├──► lumi-compositor (Wayland 显示)
       ├──► netd (网络管理)
       ├──► audiod (PipeWire 音频)
       ├──► sensord (传感器)
       ├──► telephonyd (基带/RIL)
       ├──► bluetoothd (蓝牙)
       └──► lumi-shell (用户界面)
            └──► 可选: android-container (APK 运行时)
```

## 3. 分区方案

```
boot      (64MB)   - 内核 + initramfs + DTB
system    (4GB)    - 只读系统分区 (/system)
data      (剩余)   - 用户数据 (/data)
recovery  (64MB)   - 恢复模式
```

- 文件系统: ext4 (data) / erofs (system, 只读压缩)
- 支持 A/B 无缝升级 (system_a / system_b)

## 4. Init 系统 (lumid)

### 4.1 设计

lumid 是 LumiOS 的 PID 1 进程，职责:
- 系统初始化 (挂载文件系统, 设置 cgroups, 加载模块)
- 服务管理 (启动/停止/重启/监控)
- 依赖解析 (服务间依赖图)
- 信号处理 (回收孤儿进程)
- 电源管理 (休眠/唤醒)

### 4.2 服务定义格式

```ini
# /etc/lumid/services/lumi-compositor.svc
[service]
name = lumi-compositor
description = Wayland 合成器
exec = /usr/bin/lumi-compositor
user = display
group = display

[dependencies]
requires = devd dbusd
after = devd dbusd
before = lumi-shell

[restart]
policy = always
delay = 1000
max_retries = 5

[cgroup]
memory_max = 512M
cpu_weight = 200

[environment]
XDG_RUNTIME_DIR = /run/user/1000
WAYLAND_DISPLAY = wayland-0
```

### 4.3 控制接口

```bash
lumictl start <service>
lumictl stop <service>
lumictl restart <service>
lumictl status [service]
lumictl log <service>
lumictl enable <service>    # 开机自启
lumictl disable <service>   # 取消自启
```

通过 Unix domain socket (`/run/lumid.sock`) 进行 IPC。

## 5. 包管理系统 (.lmpk)

### 5.1 包格式

`.lmpk` 文件是一个 zstd 压缩的 tar 归档:

```
package.lmpk (tar.zst)
├── .PKGINFO          # 包元数据 (TOML)
├── .PKGFILES         # 文件清单 + SHA256
├── .PRE_INSTALL      # 安装前脚本 (可选)
├── .POST_INSTALL     # 安装后脚本 (可选)
├── .PRE_REMOVE       # 卸载前脚本 (可选)
├── .POST_REMOVE      # 卸载后脚本 (可选)
└── data/             # 实际文件树
    ├── usr/
    │   ├── bin/
    │   ├── lib/
    │   └── share/
    └── etc/
```

### 5.2 .PKGINFO 格式

```toml
[package]
name = "firefox"
version = "125.0.1"
release = 1
arch = "aarch64"
description = "Mozilla Firefox 浏览器"
url = "https://www.mozilla.org/firefox/"
license = "MPL-2.0"
size = 85000000          # 安装后大小 (bytes)
packager = "LumiOS Team"

[dependencies]
depends = ["gtk4>=4.12", "nss>=3.90", "mesa>=24.0"]
makedepends = ["rust", "cbindgen", "nodejs"]
optdepends = ["ffmpeg: 视频支持", "hunspell: 拼写检查"]
conflicts = ["firefox-esr"]
provides = ["www-browser"]
replaces = []

[install]
groups = ["internet"]
backup = ["etc/firefox/policies.json"]
```

### 5.3 仓库系统

```
仓库结构:
https://repo.lumios.dev/stable/aarch64/
├── core/           # 核心系统包
├── extra/          # 额外软件
├── community/      # 社区贡献
├── android/        # Android 兼容层包
├── REPO.db         # 包索引数据库 (SQLite)
└── REPO.sig        # GPG 签名
```

## 6. 图形子系统

### 6.1 Wayland 合成器 (lumi-compositor)

基于 wlroots 库构建，功能:
- DRM/KMS 直接显示管理
- 触摸事件处理 (多指手势)
- 窗口管理 (全屏/分屏/浮动)
- VSync + 帧速率控制
- GPU 合成 (EGL/Vulkan)
- 屏幕旋转
- 虚拟键盘输入法支持
- 截图/录屏 API

### 6.2 渲染管线

```
App → Wayland Buffer → lumi-compositor → DRM/KMS → Display
                            │
                      GPU Composition
                      (EGL/Vulkan)
```

### 6.3 Shell UI (lumi-shell)

Shell 作为 Wayland 客户端运行:

```
lumi-shell
├── 锁屏 (PIN/指纹/面部)
├── 主屏幕 (应用网格/小组件)
├── 状态栏 (时间/电池/信号/Wi-Fi)
├── 通知中心 (下拉面板)
├── 快捷设置 (下拉二级面板)
├── 多任务 (最近应用)
├── 导航栏 (手势/按钮)
└── 应用启动器 (搜索/分类)
```

## 7. Android 兼容层

### 7.1 架构

```
┌─────────────────────────────────┐
│        LumiOS 原生环境            │
│  ┌───────────────────────────┐  │
│  │   android-container       │  │
│  │  ┌─────────────────────┐  │  │
│  │  │  Android Runtime    │  │  │
│  │  │  (ART + Framework)  │  │  │
│  │  │  ┌───────────────┐  │  │  │
│  │  │  │   APK 应用     │  │  │  │
│  │  │  └───────────────┘  │  │  │
│  │  └─────────────────────┘  │  │
│  │  mount ns │ pid ns │ net ns│  │
│  └───────────────────────────┘  │
│                                  │
│  binder-proxy ←→ Binder Driver  │
│  display-bridge ←→ Wayland      │
│  input-bridge ←→ libinput       │
│  audio-bridge ←→ PipeWire       │
└─────────────────────────────────┘
```

### 7.2 实现策略

1. **容器化**: 使用 Linux namespaces (mount/pid/net/ipc/uts) 隔离 Android 运行时
2. **Binder**: 加载 binder 内核模块，通过 proxy 将 Android Binder 调用桥接到 LumiOS 服务
3. **显示**: Android SurfaceFlinger → Wayland surface 映射
4. **输入**: libinput 事件 → Android InputDispatcher
5. **音频**: Android AudioFlinger → PipeWire sink
6. **存储**: bind mount 共享 /sdcard 目录
7. **网络**: 共享宿主网络命名空间或 veth 桥接

### 7.3 兼容目标

- Android API Level 34 (Android 14)
- ARM64 原生库
- Google Play Services 可选 (通过 microG 替代)
- 硬件加速 (GPU passthrough)

## 8. 设备适配

### 8.1 适配层次

```
Level 0: generic-arm64 (QEMU 虚拟机, 开发/测试用)
Level 1: 主线设备 (Google Pixel 系列, 接近主线内核)
Level 2: 厂商设备 (Xiaomi 14 Pro, 需要厂商内核补丁)
```

### 8.2 设备配置文件

每个设备目录包含:
```
devices/<device>/
├── device.mk           # 设备 Makefile
├── kernel.config        # 内核配置
├── device-tree/         # 设备树 (DTS)
├── firmware/            # 固件 blob 列表
├── fstab               # 分区表
├── init.device.svc     # 设备特定服务
└── flash.sh            # 刷写脚本
```

## 9. 安全模型 / Security Model

> 详细设计见 [security.md](security.md)

纵深防御六层架构:

1. **Hardware root of trust / 硬件信任根**: TPM/TEE/Secure Element
2. **Verified boot / 验证启动**: AVB 2.0 签名链, rollback protection
3. **Kernel hardening / 内核加固**: KASLR + CFI + stack canary + SLAB randomization
4. **System integrity / 系统完整性**: dm-verity + erofs 只读分区
5. **MAC / 强制访问控制**: AppArmor profiles per service/app
6. **App sandbox / 应用沙箱**: UID + mount ns + PID ns + net ns + seccomp-bpf + Landlock

关键差异化特性:
- **网络权限默认关闭** (Android 默认允许)
- **Landlock 文件系统隔离** (比 Android SELinux 更细粒度)
- **LUKS2 + Argon2id 全盘加密** (抗暴力破解)
- **系统级 tracker blocker** (无需第三方工具)

## 10. 性能优化架构 / Performance Architecture

> 详细设计见 [performance.md](performance.md)

### 10.1 核心性能目标

| 指标 | 目标值 | 安卓典型值 |
|---|---|---|
| 冷启动到锁屏 | < 3s | 15-30s |
| 触摸到显示延迟 | < 8ms | 20-50ms |
| UI 帧预算 @120Hz | < 4ms | 8-12ms |
| 空闲内存占用 | < 256MB | 2-4GB |
| 待机耗电 | < 0.5%/h | 1-3%/h |

### 10.2 安卓痛点解决方案总结

- **零 GC 系统层**: 全 C/C++ 核心, 无 VM 停顿
- **极简调用栈**: App→toolkit→compositor→kernel (3层 vs Android 6+层)
- **高效 IPC**: Unix socket + 共享内存 (<5μs vs Binder 50-200μs)
- **cgroup v2 强制隔离**: 后台自动冻结+限频+限 IO
- **ZRAM 压缩内存**: zstd 算法, 等效 +50% 可用 RAM
- **主动热管理**: 趋势预测而非阈值, UI 永驻大核
- **erofs + F2FS**: 系统分区零碎片, 数据分区实时 TRIM

### 10.3 渲染管线

```
Input → App Draw → Compositor Compose → DRM Flip → Display
0ms     0-4ms      4-6ms                6-8ms      VSync
```

- Triple buffering + mailbox present mode
- Direct scanout (全屏应用绕过 compositor)
- Damage tracking (只重绘变化区域)
- Touch boost (触摸时 CPU 提频 + 120Hz)

## 11. UI 设计语言 / UI Design Language

> 详细规范见 [ui-design.md](ui-design.md)

### 11.1 液态玻璃 (Liquid Glass)

LumiOS 的核心视觉材质:
- **高斯模糊**: Kawase blur 算法, 4-pass GPU shader, <1ms @1080p
- **动态着色**: 自动提取背景主色调融入玻璃
- **折射效果**: 陀螺仪驱动的内容偏移
- **环境光反射**: 玻璃表面 specular 高光
- 五级模糊层次: Ultra-thin → Thin → Regular → Thick → Chromatic

### 11.2 弹簧物理动画

所有动画基于物理弹簧模型 (stiffness/damping/mass), 非贝塞尔曲线:
- 直接操控: 触摸元素 0ms 跟随
- 自然减速 + 过冲回弹
- 视差效果: 不同层次不同速度

### 11.3 组件体系

```
状态栏 (44px, ultra-thin glass)
────────────────────────────────
主内容区 (玻璃卡片布局)
  ┌─────────────────────────┐
  │  Glass Card (16px radius)│
  │  30px blur, 0.72 opacity│
  └─────────────────────────┘
────────────────────────────────
导航栏 (48px, ultra-thin glass)
  ◇ 手势条 (134x5px, 圆角)
```

## 12. 详细设计文档索引 / Detailed Design Documents

| 文档 | 内容 |
|---|---|
| [architecture.md](architecture.md) | 本文 - 系统总体架构 |
| [performance.md](performance.md) | 性能优化详细设计 |
| [ui-design.md](ui-design.md) | UI 设计规范 (液态玻璃/动效/组件) |
| [security.md](security.md) | 安全架构详细设计 |
| [lmpk-spec.md](lmpk-spec.md) | .lmpk 包格式规范 |
| device-porting.md | 设备移植指南 (TODO) |
| android-compat.md | Android 兼容层文档 (TODO) |
