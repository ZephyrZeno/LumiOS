#!/bin/bash
# test-vm-x86.sh - Build static x86_64 binaries and test with QEMU KVM
# 构建静态 x86_64 二进制并用 QEMU KVM 测试
set -e

PROJ="/mnt/z/lacrus-projects/LumiOS"
BUILD="$HOME/lumios-vm-x86"
INITRD="$BUILD/initramfs"
KERNEL="/mnt/c/Program Files/WSL/tools/kernel"
TOP="$PROJ"

echo "=== LumiOS x86_64 VM Test ==="

mkdir -p "$BUILD"

# Step 1: Compile static x86_64 lumid
echo "--- Compiling lumid (static x86_64) ---"
LUMID="$TOP/system/core/lumid"
mkdir -p "$BUILD/lumid"
SRCS="main.c service.c supervisor.c config.c cgroup.c socket.c mount.c log.c util.c"
OBJS=""
for s in $SRCS; do
    gcc -O2 -Wall -std=gnu11 -Wno-unused-parameter -Wno-unused-function \
        -I"$LUMID/include" -c "$LUMID/src/$s" -o "$BUILD/lumid/${s%.c}.o"
    OBJS="$OBJS $BUILD/lumid/${s%.c}.o"
done
gcc -static -o "$BUILD/lumid/lumid" $OBJS
echo "  OK: $(file $BUILD/lumid/lumid | grep -o 'statically linked')"

# Step 2: Compile static x86_64 netd
echo "--- Compiling netd (static x86_64) ---"
NETD="$TOP/system/services/netd"
mkdir -p "$BUILD/netd"
SRCS="main.c netd.c wifi.c dhcp.c dns.c monitor.c"
OBJS=""
for s in $SRCS; do
    gcc -O2 -Wall -std=gnu11 -Wno-unused-parameter -Wno-unused-function \
        -I"$NETD/include" -c "$NETD/src/$s" -o "$BUILD/netd/${s%.c}.o"
    OBJS="$OBJS $BUILD/netd/${s%.c}.o"
done
gcc -static -o "$BUILD/netd/netd" $OBJS
echo "  OK: $(file $BUILD/netd/netd | grep -o 'statically linked')"

# Step 3: Compile static x86_64 android-container
echo "--- Compiling android-container (static x86_64) ---"
AC="$TOP/android-compat/container"
mkdir -p "$BUILD/android"
SRCS="main.c container.c namespace.c mounts.c cgroup.c properties.c"
OBJS=""
for s in $SRCS; do
    gcc -O2 -Wall -std=gnu11 -Wno-unused-parameter -Wno-unused-function \
        -I"$AC/include" -c "$AC/src/$s" -o "$BUILD/android/${s%.c}.o"
    OBJS="$OBJS $BUILD/android/${s%.c}.o"
done
gcc -static -o "$BUILD/android/android-container" $OBJS
echo "  OK"

# Step 4: Create initramfs
echo "--- Creating initramfs ---"
rm -rf "$INITRD"
mkdir -p "$INITRD"/{bin,sbin,usr/{bin,sbin},etc/lumid/services,dev,proc,sys,run,tmp,var/{log/lumid,lib,run},root}

cp "$BUILD/lumid/lumid" "$INITRD/sbin/"
cp "$BUILD/netd/netd" "$INITRD/usr/sbin/"
cp "$BUILD/android/android-container" "$INITRD/usr/bin/"
ln -sf /sbin/lumid "$INITRD/init"

# Device nodes
sudo mknod -m 666 "$INITRD/dev/null"    c 1 3
sudo mknod -m 666 "$INITRD/dev/zero"    c 1 5
sudo mknod -m 666 "$INITRD/dev/console" c 5 1
sudo mknod -m 666 "$INITRD/dev/tty"     c 5 0
sudo mknod -m 666 "$INITRD/dev/tty0"    c 4 0
sudo mknod -m 666 "$INITRD/dev/ttyS0"   c 4 64

# System files
cat > "$INITRD/etc/os-release" << 'EOF'
NAME="LumiOS"
VERSION="0.1.0"
ID=lumios
PRETTY_NAME="LumiOS 0.1.0"
EOF
echo "lumios" > "$INITRD/etc/hostname"

# Service configs
for svc in "$TOP/build/rootfs/etc/lumid/services"/*.svc; do
    [ -f "$svc" ] && cp "$svc" "$INITRD/etc/lumid/services/"
done

# Add compositor and shell service for lumid to discover
cat > "$INITRD/etc/lumid/services/lumi-compositor.svc" << 'EOF'
[service]
name = lumi-compositor
description = Wayland Compositor
exec = /usr/bin/lumi-compositor
type = simple
[dependencies]
after = devd
[restart]
policy = always
delay = 1000
max_retries = 3
[cgroup]
memory_max = 512M
cpu_weight = 200
EOF

cat > "$INITRD/etc/lumid/services/netd.svc" << 'EOF'
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

chmod 755 "$INITRD/sbin"/*
chmod 755 "$INITRD/usr/bin"/* 2>/dev/null
chmod 755 "$INITRD/usr/sbin"/* 2>/dev/null

# Pack initramfs
cd "$INITRD"
find . | cpio -o -H newc 2>/dev/null | gzip > "$BUILD/initramfs.cpio.gz"
echo "  initramfs: $(du -h "$BUILD/initramfs.cpio.gz" | cut -f1)"

# Step 5: Boot with QEMU KVM
echo ""
echo "=== Starting QEMU x86_64 with KVM ==="
echo "  Kernel: $KERNEL"
echo "  initrd: $BUILD/initramfs.cpio.gz"
echo "  Press Ctrl+A then X to exit QEMU"
echo ""

timeout 30 qemu-system-x86_64 \
    -enable-kvm \
    -cpu host \
    -smp 2 \
    -m 512M \
    -nographic \
    -kernel "$KERNEL" \
    -initrd "$BUILD/initramfs.cpio.gz" \
    -append "console=ttyS0 earlycon loglevel=7 rdinit=/init" \
    -no-reboot \
    2>&1 | tail -80
