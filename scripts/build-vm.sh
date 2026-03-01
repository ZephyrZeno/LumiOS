#!/bin/bash
# build-vm.sh - Cross-compile LumiOS and create QEMU VM image
# 交叉编译 LumiOS 并创建 QEMU VM 镜像
#
# Usage: ./scripts/build-vm.sh [--run]
#   --run: also start QEMU after building
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

TOP="$(cd "$(dirname "$0")/.." && pwd)"
# Use local ext4 filesystem for build output (HGFS/NTFS doesn't support symlinks/mknod)
# 使用本地 ext4 文件系统存放构建输出（HGFS/NTFS 不支持符号链接/设备节点）
BUILD="$HOME/lumios-vm-build"
SYSROOT="$BUILD/sysroot"
ROOTFS="$BUILD/rootfs"
IMGDIR="$BUILD/images"

# Cross-compiler / 交叉编译器
CROSS=aarch64-linux-gnu-
CC="${CROSS}gcc"
CFLAGS="-O2 -Wall -Wextra -std=gnu11 -Wno-unused-parameter -Wno-unused-function"
LDFLAGS="-static"

RUN_AFTER=false
if [ "$1" = "--run" ]; then RUN_AFTER=true; fi

log() { echo -e "${BLUE}=== $1 ===${NC}"; }
ok()  { echo -e "  ${GREEN}OK${NC} $1"; }
err() { echo -e "  ${RED}ERROR${NC} $1"; }

# Check cross-compiler / 检查交叉编译器
if ! which $CC >/dev/null 2>&1; then
    err "Cross-compiler $CC not found"
    echo "Install: sudo apt install gcc-aarch64-linux-gnu"
    exit 1
fi

mkdir -p "$BUILD" "$SYSROOT"/{sbin,usr/bin,usr/lib,usr/sbin,etc/lumid/services}
mkdir -p "$ROOTFS" "$IMGDIR"

# === Step 1: Cross-compile lumid / 交叉编译 lumid ===
log "Cross-compiling lumid (init system)"
LUMID_SRC="$TOP/system/core/lumid"
LUMID_OUT="$BUILD/lumid"
mkdir -p "$LUMID_OUT"

LUMID_SRCS="main.c service.c supervisor.c config.c cgroup.c socket.c mount.c log.c util.c"
LUMID_OBJS=""
for src in $LUMID_SRCS; do
    obj="$LUMID_OUT/$(basename $src .c).o"
    $CC $CFLAGS -I"$LUMID_SRC/include" -c "$LUMID_SRC/src/$src" -o "$obj" 2>/dev/null
    LUMID_OBJS="$LUMID_OBJS $obj"
done
$CC $CFLAGS $LDFLAGS -o "$LUMID_OUT/lumid" $LUMID_OBJS 2>/dev/null && ok "lumid ($(file $LUMID_OUT/lumid | grep -o 'ARM aarch64' || echo 'built'))"

# lumictl
CTL_SRCS="lumictl.c socket.c util.c log.c"
CTL_OBJS=""
for src in $CTL_SRCS; do
    obj="$LUMID_OUT/ctl_$(basename $src .c).o"
    $CC $CFLAGS -I"$LUMID_SRC/include" -DLUMICTL_BUILD -c "$LUMID_SRC/src/$src" -o "$obj" 2>/dev/null
    CTL_OBJS="$CTL_OBJS $obj"
done
$CC $CFLAGS $LDFLAGS -o "$LUMID_OUT/lumictl" $CTL_OBJS 2>/dev/null && ok "lumictl"

# === Step 2: Cross-compile lmpkg / 交叉编译 lmpkg ===
log "Cross-compiling lmpkg (package manager)"
LMPKG_SRC="$TOP/packages/lmpkg"
LMPKG_OUT="$BUILD/lmpkg"
mkdir -p "$LMPKG_OUT"

LMPKG_SRCS="main.c database.c package.c install.c remove.c sync.c depsolve.c version.c util.c"
LMPKG_OBJS=""
LMPKG_OK=true
for src in $LMPKG_SRCS; do
    obj="$LMPKG_OUT/$(basename $src .c).o"
    if ! $CC $CFLAGS -I"$LMPKG_SRC/include" -c "$LMPKG_SRC/src/$src" -o "$obj" 2>/dev/null; then
        LMPKG_OK=false
    else
        LMPKG_OBJS="$LMPKG_OBJS $obj"
    fi
done
if $LMPKG_OK; then
    $CC $CFLAGS $LDFLAGS -o "$LMPKG_OUT/lmpkg" $LMPKG_OBJS 2>/dev/null && ok "lmpkg" || \
        err "lmpkg link failed (missing cross libarchive, skipping)"
else
    err "lmpkg compile failed (missing cross libarchive headers, skipping)"
fi

# === Step 3: Cross-compile android-container / 交叉编译 Android 容器 ===
log "Cross-compiling android-container"
AC_SRC="$TOP/android-compat/container"
AC_OUT="$BUILD/android"
mkdir -p "$AC_OUT"

AC_SRCS="main.c container.c namespace.c mounts.c cgroup.c properties.c"
AC_OBJS=""
for src in $AC_SRCS; do
    obj="$AC_OUT/$(basename $src .c).o"
    $CC $CFLAGS -I"$AC_SRC/include" -c "$AC_SRC/src/$src" -o "$obj" 2>/dev/null
    AC_OBJS="$AC_OBJS $obj"
done
$CC $CFLAGS $LDFLAGS -o "$AC_OUT/android-container" $AC_OBJS 2>/dev/null && ok "android-container"

# === Step 4: Cross-compile netd / 交叉编译 netd ===
log "Cross-compiling netd (network manager)"
NETD_SRC="$TOP/system/services/netd"
NETD_OUT="$BUILD/netd"
mkdir -p "$NETD_OUT"

NETD_SRCS="main.c netd.c wifi.c dhcp.c dns.c monitor.c"
NETD_OBJS=""
for src in $NETD_SRCS; do
    obj="$NETD_OUT/$(basename $src .c).o"
    $CC $CFLAGS -I"$NETD_SRC/include" -c "$NETD_SRC/src/$src" -o "$obj" 2>/dev/null
    NETD_OBJS="$NETD_OBJS $obj"
done
$CC $CFLAGS $LDFLAGS -o "$NETD_OUT/netd" $NETD_OBJS 2>/dev/null && ok "netd"

# === Step 5: Install to sysroot / 安装到 sysroot ===
log "Installing binaries to sysroot"
cp "$LUMID_OUT/lumid"    "$SYSROOT/sbin/" 2>/dev/null && ok "sbin/lumid"
cp "$LUMID_OUT/lumictl"  "$SYSROOT/usr/bin/" 2>/dev/null && ok "usr/bin/lumictl"
[ -f "$LMPKG_OUT/lmpkg" ] && cp "$LMPKG_OUT/lmpkg" "$SYSROOT/usr/bin/" && ok "usr/bin/lmpkg"
cp "$AC_OUT/android-container" "$SYSROOT/usr/bin/" 2>/dev/null && ok "usr/bin/android-container"
cp "$NETD_OUT/netd"      "$SYSROOT/usr/sbin/" 2>/dev/null && ok "usr/sbin/netd"

# === Step 6: Generate rootfs / 生成根文件系统 ===
log "Generating root filesystem"

# Create directory structure / 创建目录结构
mkdir -p "$ROOTFS"/{bin,sbin,usr/{bin,lib,sbin,share},etc/{lumid/services,network},var/{lib,log,cache,run},tmp,dev,proc,sys,run,mnt,opt,data,boot,root}
mkdir -p "$ROOTFS"/var/lib/{lmpkg/{local,sync},android,audiod}
mkdir -p "$ROOTFS"/var/cache/lmpkg
mkdir -p "$ROOTFS"/var/log/lumid
mkdir -p "$ROOTFS"/run/{lumid,user/1000}

# Copy binaries / 复制二进制
cp "$SYSROOT/sbin"/* "$ROOTFS/sbin/" 2>/dev/null || true
cp "$SYSROOT/usr/bin"/* "$ROOTFS/usr/bin/" 2>/dev/null || true
cp "$SYSROOT/usr/sbin"/* "$ROOTFS/usr/sbin/" 2>/dev/null || true

# Install LumiScript built-in apps (.lmpk) / 安装 LumiScript 内置应用
mkdir -p "$ROOTFS/usr/share/lumios/apps"
if [ -d "$TOP/system/apps" ]; then
    cp "$TOP/system/apps"/*.lmpk "$ROOTFS/usr/share/lumios/apps/" 2>/dev/null && \
        ok "Installed $(ls "$TOP/system/apps"/*.lmpk 2>/dev/null | wc -l) built-in apps" || true
fi

# Essential symlinks (remove existing dirs first) / 必要符号链接（先删除已有目录）
rm -rf "$ROOTFS/init" "$ROOTFS/lib" "$ROOTFS/lib64"
ln -sf /sbin/lumid "$ROOTFS/init"
ln -sf /usr/lib "$ROOTFS/lib"
ln -sf /usr/lib "$ROOTFS/lib64"

# Create a minimal busybox-like /bin/sh (we'll use a static shell)
# 创建最小的 /bin/sh
# For VM testing we need at least a shell - download static busybox
log "Downloading static busybox for aarch64"
BUSYBOX_URL="https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox"
# For aarch64 we need the right binary - use a simpler approach
# Create a minimal init script that lumid can use
cat > "$ROOTFS/etc/os-release" << 'EOF'
NAME="LumiOS"
VERSION="0.1.0"
ID=lumios
PRETTY_NAME="LumiOS 0.1.0"
EOF

echo "lumios" > "$ROOTFS/etc/hostname"

cat > "$ROOTFS/etc/passwd" << 'EOF'
root:x:0:0:root:/root:/bin/sh
display:x:1000:1000:Display:/home/display:/bin/sh
nobody:x:65534:65534:Nobody:/nonexistent:/usr/sbin/nologin
EOF

cat > "$ROOTFS/etc/group" << 'EOF'
root:x:0:
display:x:1000:
audio:x:29:display
video:x:44:display
input:x:104:display
nobody:x:65534:
EOF

# Copy service configs / 复制服务配置
cp "$TOP/build/rootfs/etc/lumid/services"/*.svc "$ROOTFS/etc/lumid/services/" 2>/dev/null || true
# Also copy from mkrootfs generated ones
cat > "$ROOTFS/etc/lumid/services/lumi-compositor.svc" << 'EOF'
[service]
name = lumi-compositor
description = Wayland Compositor
exec = /usr/bin/lumi-compositor
type = simple
user = display

[dependencies]
after = devd

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
EOF

cat > "$ROOTFS/etc/lumid/services/netd.svc" << 'EOF'
[service]
name = netd
description = Network Manager
exec = /usr/sbin/netd
type = simple

[dependencies]
after = devd

[restart]
policy = always
delay = 2000
max_retries = 5

[cgroup]
memory_max = 64M
cpu_weight = 50
EOF

# fstab
cat > "$ROOTFS/etc/fstab" << 'EOF'
/dev/vda2  /      ext4   rw,noatime     0 1
tmpfs      /tmp   tmpfs  nosuid,nodev   0 0
tmpfs      /run   tmpfs  nosuid,nodev   0 0
proc       /proc  proc   defaults       0 0
sysfs      /sys   sysfs  defaults       0 0
EOF

# Set permissions / 设置权限
chmod 755 "$ROOTFS/sbin"/* 2>/dev/null || true
chmod 755 "$ROOTFS/usr/bin"/* 2>/dev/null || true
chmod 755 "$ROOTFS/usr/sbin"/* 2>/dev/null || true
chmod 1777 "$ROOTFS/tmp"

ok "rootfs generated at $ROOTFS"

# === Step 7: Create disk image / 创建磁盘镜像 ===
log "Creating QEMU disk image"

IMAGE="$IMGDIR/lumios.img"

# Create 256MB image (enough for minimal system)
# 创建 256MB 镜像（足够最小系统）
truncate -s 256M "$IMAGE"

# Create single ext4 partition (simplified for testing)
# 创建单个 ext4 分区（简化用于测试）
mkfs.ext4 -q -d "$ROOTFS" -L lumios "$IMAGE" 2>/dev/null && ok "disk image: $IMAGE (256MB)" || {
    # Fallback: manual mount + copy / 后备: 手动挂载+复制
    mkfs.ext4 -q -F -L lumios "$IMAGE"
    MOUNT_TMP=$(mktemp -d)
    sudo mount -o loop "$IMAGE" "$MOUNT_TMP"
    sudo cp -a "$ROOTFS"/* "$MOUNT_TMP"/
    sudo umount "$MOUNT_TMP"
    rmdir "$MOUNT_TMP"
    ok "disk image (fallback method): $IMAGE"
}

# === Step 8: Verify / 验证 ===
log "Build Summary"
echo ""
echo "  Binaries:"
for f in "$ROOTFS/sbin/lumid" "$ROOTFS/usr/bin/lumictl" "$ROOTFS/usr/bin/android-container" "$ROOTFS/usr/sbin/netd"; do
    if [ -f "$f" ]; then
        ARCH=$(file "$f" 2>/dev/null | grep -o 'ARM aarch64' || echo 'unknown')
        SIZE=$(du -h "$f" | cut -f1)
        echo -e "    ${GREEN}✓${NC} $(basename $f) ($ARCH, $SIZE)"
    fi
done

echo ""
echo "  Image: $IMAGE ($(du -h "$IMAGE" | cut -f1))"
echo "  Rootfs: $ROOTFS ($(du -sh "$ROOTFS" | cut -f1))"
echo ""

# === Step 9: Run QEMU (if --run) / 运行 QEMU ===
if $RUN_AFTER; then
    log "Starting QEMU VM"

    # Download aarch64 kernel if not present / 下载 aarch64 内核
    KERNEL="$BUILD/Image"
    if [ ! -f "$KERNEL" ]; then
        log "Downloading aarch64 Linux kernel"
        # Use Ubuntu's linux-image-generic-arm64 kernel or download prebuilt
        # 使用 Ubuntu 的 arm64 内核或下载预编译版
        KERNEL_DEB_URL="http://ports.ubuntu.com/pool/main/l/linux/linux-image-6.8.0-51-generic_6.8.0-51.52_arm64.deb"
        KERNEL_TMP=$(mktemp -d)
        # Try extracting kernel from the host system's qemu support files first
        # 优先尝试从宿主系统的 QEMU 支持文件提取内核
        if [ -f /boot/vmlinuz-*-generic ]; then
            # Host kernel won't work (x86_64), need aarch64 kernel
            true
        fi
        # Download a minimal prebuilt kernel / 下载预编译最小内核
        if apt list --installed 2>/dev/null | grep -q linux-image.*arm64; then
            dpkg -x /var/cache/apt/archives/linux-image-*arm64*.deb "$KERNEL_TMP" 2>/dev/null
            find "$KERNEL_TMP" -name "vmlinuz-*" -exec cp {} "$KERNEL" \;
        fi
        if [ ! -f "$KERNEL" ]; then
            # Fallback: build a minimal kernel or download one
            # 后备: 使用 Debian/Ubuntu 的 arm64 内核包
            apt-get download linux-image-generic-arm64 2>/dev/null || true
            if ls linux-image-*arm64*.deb 1>/dev/null 2>&1; then
                dpkg -x linux-image-*arm64*.deb "$KERNEL_TMP"
                find "$KERNEL_TMP" -name "vmlinuz-*" -exec cp {} "$KERNEL" \;
                rm -f linux-image-*arm64*.deb
            fi
        fi
        if [ ! -f "$KERNEL" ]; then
            # Last resort: use installed kernel from apt cache
            # 最后手段: 直接安装 arm64 内核包并提取
            apt-get install -y --download-only linux-image-6.8.0-51-generic:arm64 2>/dev/null || true
            if ls /var/cache/apt/archives/linux-image-*arm64*.deb 1>/dev/null 2>&1; then
                dpkg -x /var/cache/apt/archives/linux-image-*arm64*.deb "$KERNEL_TMP"
                find "$KERNEL_TMP" -name "vmlinuz-*" -exec cp {} "$KERNEL" \;
            fi
        fi
        rm -rf "$KERNEL_TMP"
    fi

    if [ ! -f "$KERNEL" ]; then
        err "No aarch64 kernel found. Install manually:"
        echo "  sudo apt install linux-image-generic:arm64"
        echo "  Or download a prebuilt Image to: $KERNEL"
        exit 1
    fi

    echo "  Kernel: $KERNEL"
    echo "  NOTE: lumid will start as PID 1 (or drop to shell on failure)"
    echo "  Press Ctrl+A then X to exit QEMU"
    echo ""

    qemu-system-aarch64 \
        -machine virt \
        -cpu cortex-a57 \
        -smp 2 \
        -m 512M \
        -nographic \
        -kernel "$KERNEL" \
        -drive file="$IMAGE",format=raw,if=virtio \
        -append "root=/dev/vda rw console=ttyAMA0 init=/sbin/lumid loglevel=7" \
        -netdev user,id=net0 \
        -device virtio-net-pci,netdev=net0 \
        2>&1 || true
else
    echo ""
    echo -e "${GREEN}Build complete!${NC} To start the VM:"
    echo ""
    echo "  # In WSL2:"
    echo "  cd /mnt/z/lacrus-projects/LumiOS"
    echo "  bash scripts/build-vm.sh --run"
    echo ""
    echo "  # Or manually:"
    echo "  qemu-system-aarch64 -machine virt -cpu cortex-a57 -smp 2 -m 512M -nographic \\"
    echo "    -drive file=$IMAGE,format=raw,if=virtio \\"
    echo "    -append 'root=/dev/vda rw console=ttyAMA0 init=/sbin/lumid'"
    echo ""
fi
