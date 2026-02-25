#!/bin/bash
# mk-initramfs.sh - Create minimal initramfs with lumid as init
# 创建包含 lumid 作为 init 的最小 initramfs
set -e

TOP="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$TOP/out/vm-build"
INITRD="$BUILD/initramfs"
ROOTFS="$BUILD/rootfs"

echo "=== Creating initramfs ==="

rm -rf "$INITRD"
mkdir -p "$INITRD"/{bin,sbin,usr/bin,usr/sbin,etc/lumid/services,dev,proc,sys,run,tmp,var/log/lumid,var/lib,root}

# Copy our binaries / 复制二进制文件
cp "$ROOTFS/sbin/lumid" "$INITRD/sbin/" 2>/dev/null || true
cp "$ROOTFS/usr/bin/lumictl" "$INITRD/usr/bin/" 2>/dev/null || true
cp "$ROOTFS/usr/bin/android-container" "$INITRD/usr/bin/" 2>/dev/null || true
cp "$ROOTFS/usr/sbin/netd" "$INITRD/usr/sbin/" 2>/dev/null || true

# Symlinks / 符号链接
ln -sf /sbin/lumid "$INITRD/init"
ln -sf /usr/lib "$INITRD/lib"
ln -sf /usr/lib "$INITRD/lib64"

# Essential device nodes / 必要设备节点
mknod -m 666 "$INITRD/dev/null"    c 1 3 2>/dev/null || true
mknod -m 666 "$INITRD/dev/zero"    c 1 5 2>/dev/null || true
mknod -m 666 "$INITRD/dev/console" c 5 1 2>/dev/null || true
mknod -m 666 "$INITRD/dev/tty"     c 5 0 2>/dev/null || true
mknod -m 666 "$INITRD/dev/ttyAMA0" c 204 64 2>/dev/null || true

# System files / 系统文件
cp "$ROOTFS/etc/os-release" "$INITRD/etc/" 2>/dev/null || true
cp "$ROOTFS/etc/hostname"   "$INITRD/etc/" 2>/dev/null || true
cp "$ROOTFS/etc/passwd"     "$INITRD/etc/" 2>/dev/null || true
cp "$ROOTFS/etc/group"      "$INITRD/etc/" 2>/dev/null || true

# Service configs / 服务配置
cp "$ROOTFS/etc/lumid/services"/*.svc "$INITRD/etc/lumid/services/" 2>/dev/null || true

# Create cpio archive / 创建 cpio 归档
cd "$INITRD"
find . | cpio -o -H newc 2>/dev/null | gzip > "$BUILD/initramfs.cpio.gz"

echo "  initramfs: $(du -h "$BUILD/initramfs.cpio.gz" | cut -f1)"
echo "  Contents:"
find . -type f | head -20 | sed 's/^/    /'
echo "=== Done ==="
