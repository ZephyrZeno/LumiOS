# LumiOS 编译与测试指南

## 目录

- [环境要求](#环境要求)
- [依赖安装](#依赖安装)
- [编译合成器](#编译合成器)
- [编译 Shell](#编译-shell)
- [启动测试](#启动测试)
- [竖屏模式](#竖屏模式)
- [常见问题](#常见问题)

---

## 环境要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Ubuntu 24.04 LTS (x86_64) |
| 编译器 | GCC 13+ |
| 构建系统 | Meson 1.3+ / Ninja 1.11+ |
| 显示服务器 | X11 (Xorg) 或 Wayland |
| 虚拟机 | VMware (推荐) / VirtualBox |

## 依赖安装

```bash
# 基础构建工具
sudo apt install -y build-essential meson ninja-build pkg-config

# Wayland + wlroots
sudo apt install -y libwayland-dev wayland-protocols libwlroots-dev

# 输入/显示
sudo apt install -y libxkbcommon-dev libinput-dev libpixman-1-dev
sudo apt install -y libegl-dev libgles2-mesa-dev libdrm-dev libgbm-dev

# Cairo + Pango (Shell 渲染)
sudo apt install -y libcairo2-dev libpango1.0-dev

# GLib + GdkPixbuf (壁纸加载)
sudo apt install -y libglib2.0-dev libgdk-pixbuf-2.0-dev

# 如果 libgdk-pixbuf-2.0-dev 安装失败 (依赖冲突):
sudo apt install -y --allow-downgrades libgdk-pixbuf-2.0-dev \
    libdeflate0=1.19-1build1 libdeflate-dev=1.19-1build1
```

## 编译合成器

```bash
cd /mnt/hgfs/lacrus-projects/LumiOS

# 清理旧构建
rm -rf out/test/compositor

# 配置
cd system/core/lumi-compositor
meson setup /mnt/hgfs/lacrus-projects/LumiOS/out/test/compositor --prefix=/usr

# 编译
ninja -C /mnt/hgfs/lacrus-projects/LumiOS/out/test/compositor

# 验证
ls -la /mnt/hgfs/lacrus-projects/LumiOS/out/test/compositor/lumi-compositor
```

## 编译 Shell

```bash
cd /mnt/hgfs/lacrus-projects/LumiOS

# 清理旧构建
rm -rf out/test/shell

# 配置
cd system/core/lumi-shell
meson setup /mnt/hgfs/lacrus-projects/LumiOS/out/test/shell --prefix=/usr

# 编译
ninja -C /mnt/hgfs/lacrus-projects/LumiOS/out/test/shell

# 验证
ls -la /mnt/hgfs/lacrus-projects/LumiOS/out/test/shell/lumi-shell
```

### 一键编译脚本

```bash
cd /mnt/hgfs/lacrus-projects/LumiOS

# 合成器
rm -rf out/test/compositor
cd system/core/lumi-compositor
meson setup /mnt/hgfs/lacrus-projects/LumiOS/out/test/compositor --prefix=/usr
ninja -C /mnt/hgfs/lacrus-projects/LumiOS/out/test/compositor 2>&1 | tail -5

# Shell
cd /mnt/hgfs/lacrus-projects/LumiOS
rm -rf out/test/shell
cd system/core/lumi-shell
meson setup /mnt/hgfs/lacrus-projects/LumiOS/out/test/shell --prefix=/usr
ninja -C /mnt/hgfs/lacrus-projects/LumiOS/out/test/shell 2>&1 | tail -5
```

## 启动测试

### 横屏模式（默认）

```bash
cd /mnt/hgfs/lacrus-projects/LumiOS
killall lumi-compositor lumi-shell 2>/dev/null
rm -f /run/user/1000/wayland-lumi.lock
sleep 1

# 启动合成器 (后台)
WLR_BACKENDS=x11 \
WLR_RENDERER=pixman \
LIBGL_ALWAYS_SOFTWARE=1 \
./out/test/compositor/lumi-compositor -s wayland-lumi 2>&1 &

sleep 2

# 启动 Shell
WAYLAND_DISPLAY=wayland-lumi ./out/test/shell/lumi-shell 2>&1
```

### 停止测试

```bash
killall lumi-compositor lumi-shell 2>/dev/null
```

## 竖屏模式

X11 后端默认不支持自定义分辨率。以下是几种竖屏测试方案：

### 方案 A：使用测试脚本（推荐）

```bash
cd /mnt/hgfs/lacrus-projects/LumiOS
chmod +x scripts/test-portrait.sh
./scripts/test-portrait.sh 450x900
```

脚本会自动：
1. 用 `cvt` + `xrandr --newmode` 添加竖屏分辨率
2. 启动合成器并传入 `-r 450x900`
3. 如果自定义模式失败，自动回退到默认分辨率

### 方案 B：手动 xrandr 添加竖屏模式

```bash
# 1. 生成 modeline
cvt 450 900 60

# 2. 添加模式 (用 cvt 输出的 Modeline 行)
xrandr --newmode "450x900_60.00" 25.25 450 472 520 590 900 903 913 931 -hsync +vsync

# 3. 找到当前输出名
xrandr --query | grep ' connected'
# 例如: Virtual-1 connected ...

# 4. 添加到输出
xrandr --addmode Virtual-1 "450x900_60.00"

# 5. 启动合成器
cd /mnt/hgfs/lacrus-projects/LumiOS
killall lumi-compositor lumi-shell 2>/dev/null
rm -f /run/user/1000/wayland-lumi.lock
sleep 1
WLR_BACKENDS=x11 WLR_RENDERER=pixman LIBGL_ALWAYS_SOFTWARE=1 \
./out/test/compositor/lumi-compositor -s wayland-lumi -r 450x900 2>&1 &
sleep 2
WAYLAND_DISPLAY=wayland-lumi ./out/test/shell/lumi-shell 2>&1
```

### 方案 C：指定分辨率参数

合成器支持 `-r WxH` 参数指定分辨率：

```bash
# 竖屏手机
./out/test/compositor/lumi-compositor -s wayland-lumi -r 450x1000

# 横屏平板
./out/test/compositor/lumi-compositor -s wayland-lumi -r 1280x800

# 高清竖屏 (需要 DRM 后端，X11 可能不支持)
./out/test/compositor/lumi-compositor -s wayland-lumi -r 1200x2608
```

> **注意**：X11 后端可能拒绝某些分辨率。如果日志显示 `custom mode failed, falling back`，说明该分辨率不被支持，请尝试更小的值或使用 xrandr 方案。

### 方案 D：Wayland 嵌套（如果桌面是 Wayland）

如果你的 Ubuntu 使用 Wayland 会话：

```bash
WLR_BACKENDS=wayland \
WLR_RENDERER=pixman \
./out/test/compositor/lumi-compositor -s wayland-lumi -r 450x1000 2>&1 &
sleep 2
WAYLAND_DISPLAY=wayland-lumi ./out/test/shell/lumi-shell 2>&1
```

> 检查当前会话类型：`echo $XDG_SESSION_TYPE`（输出 `x11` 或 `wayland`）

## 合成器参数

| 参数 | 说明 | 示例 |
|------|------|------|
| `-s <name>` | Wayland socket 名称 | `-s wayland-lumi` |
| `-r WxH` | 自定义分辨率 | `-r 450x1000` |
| `-n` | 禁用 GPU 特效 | `-n` |
| `-d` | 调试日志 | `-d` |
| `-h` | 显示帮助 | `-h` |

## 环境变量

| 变量 | 说明 | 值 |
|------|------|------|
| `WLR_BACKENDS` | wlroots 后端 | `x11` / `wayland` / `drm` / `headless` |
| `WLR_RENDERER` | 渲染器 | `pixman` (软件) / `gles2` (GPU) |
| `LIBGL_ALWAYS_SOFTWARE` | 强制软件 GL | `1` |
| `WAYLAND_DISPLAY` | Shell 连接的 Wayland socket | `wayland-lumi` |

## 常见问题

### Q: 编译时 `gdk-pixbuf-2.0` 找不到

```bash
sudo apt install -y --allow-downgrades libgdk-pixbuf-2.0-dev \
    libdeflate0=1.19-1build1 libdeflate-dev=1.19-1build1
```

### Q: 启动时 `cannot connect to Wayland display`

确保合成器已启动并且 `WAYLAND_DISPLAY` 设置正确：

```bash
# 先启动合成器
WLR_BACKENDS=x11 WLR_RENDERER=pixman LIBGL_ALWAYS_SOFTWARE=1 \
./out/test/compositor/lumi-compositor -s wayland-lumi 2>&1 &
sleep 2

# 再启动 Shell (必须指定 WAYLAND_DISPLAY)
WAYLAND_DISPLAY=wayland-lumi ./out/test/shell/lumi-shell
```

### Q: 竖屏分辨率不生效

X11 后端不支持所有分辨率。检查日志：

```
custom mode 450x1000 failed, falling back
```

解决：先用 xrandr 添加模式（方案 B），或使用默认横屏测试。

### Q: 动画卡顿

1. 确保使用 `pixman` 渲染器（`WLR_RENDERER=pixman`）
2. 壁纸图片会自动缩放到屏幕尺寸，无需手动调整
3. 如果仍然卡顿，检查 VM 分配的 CPU/内存是否充足

### Q: 端口/锁文件冲突

```bash
killall lumi-compositor lumi-shell 2>/dev/null
rm -f /run/user/1000/wayland-lumi.lock
```

### Q: 开机动画不显示 dmesg 日志

开机动画从 `dmesg` 读取真实内核日志。如果权限不足：

```bash
# 允许普通用户读取 dmesg
sudo sysctl kernel.dmesg_restrict=0
```

## 编译 Makefile 组件

以下组件使用 Makefile 构建（不需要 Meson）：

```bash
cd /mnt/hgfs/lacrus-projects/LumiOS

# 应用开发 SDK
cd system/core/liblumiapp && make && cd -

# 安全沙箱
cd system/core/sandbox && make && cd -

# 内置应用
cd apps/browser && make && cd -
cd apps/phone && make && cd -
cd apps/messages && make && cd -
cd apps/camera && make && cd -

# 系统服务
cd system/services/bluetoothd && make && cd -
cd system/services/sensord && make && cd -
cd system/services/telephonyd && make && cd -
```

### 一键编译所有 Makefile 组件

```bash
cd /mnt/hgfs/lacrus-projects/LumiOS
for d in system/core/liblumiapp system/core/sandbox \
         apps/browser apps/phone apps/messages apps/camera \
         system/services/bluetoothd system/services/sensord \
         system/services/telephonyd; do
    echo "=== $d ==="
    (cd "$d" && make clean 2>/dev/null; make 2>&1 | tail -3)
done
echo "=== ALL DONE ==="
```

## 项目路径

| 组件 | 源码路径 | 构建系统 | 构建输出 |
|------|----------|---------|----------|
| 合成器 | `system/core/lumi-compositor/` | Meson | `out/test/compositor/lumi-compositor` |
| Shell | `system/core/lumi-shell/` | Meson | `out/test/shell/lumi-shell` |
| 渲染层 | `system/core/lumi-render/` | Meson | (静态库) |
| UI 控件 | `system/core/lumi-toolkit/` | Meson | (静态库) |
| **应用 SDK** | `system/core/liblumiapp/` | Makefile | `build/liblumiapp.a` + `.so` |
| **沙箱** | `system/core/sandbox/` | Makefile | `build/lumi-sandbox` |
| **浏览器** | `apps/browser/` | Makefile | `build/lumi-browser` |
| **电话** | `apps/phone/` | Makefile | `build/lumi-phone` |
| **短信** | `apps/messages/` | Makefile | `build/lumi-messages` |
| **相机** | `apps/camera/` | Makefile | `build/lumi-camera` |
| **蓝牙** | `system/services/bluetoothd/` | Makefile | `build/bluetoothd` |
| **传感器** | `system/services/sensord/` | Makefile | `build/sensord` |
| **电话服务** | `system/services/telephonyd/` | Makefile | `build/telephonyd` |
| 壁纸 | `system/core/lumi-shell/share/wallpapers/` | — | — |
