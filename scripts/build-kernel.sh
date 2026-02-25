#!/bin/bash
# build-kernel.sh - Build minimal aarch64 kernel for QEMU virt
# 为 QEMU virt 构建最小 aarch64 内核
set -e

KERNEL_SRC="$HOME/lumios-kernel/linux-6.1.73"
OUTPUT="/mnt/z/lacrus-projects/LumiOS/out/vm-build/kernel"
CROSS=aarch64-linux-gnu-
JOBS=$(nproc)

echo "=== Building Linux 6.1.73 for QEMU virt aarch64 ==="
echo "  Cross: $CROSS"
echo "  Jobs:  $JOBS"

cd "$KERNEL_SRC"

# Start from defconfig then trim
make ARCH=arm64 CROSS_COMPILE=$CROSS defconfig

# Apply minimal overrides for QEMU virt
cat >> .config << 'KEOF'
# QEMU virt essentials
CONFIG_SERIAL_AMBA_PL011=y
CONFIG_SERIAL_AMBA_PL011_CONSOLE=y
CONFIG_VIRTIO=y
CONFIG_VIRTIO_PCI=y
CONFIG_VIRTIO_BLK=y
CONFIG_VIRTIO_NET=y
CONFIG_VIRTIO_CONSOLE=y
CONFIG_HW_RANDOM_VIRTIO=y
CONFIG_BLK_DEV_INITRD=y
CONFIG_RD_GZIP=y
CONFIG_DEVTMPFS=y
CONFIG_DEVTMPFS_MOUNT=y
CONFIG_EXT4_FS=y
CONFIG_TMPFS=y
CONFIG_PROC_FS=y
CONFIG_SYSFS=y
CONFIG_PRINTK=y
CONFIG_EARLY_PRINTK=y
# Disable unnecessary for faster build
# CONFIG_SOUND is not set
# CONFIG_USB is not set
# CONFIG_WLAN is not set
# CONFIG_WIRELESS is not set
# CONFIG_BT is not set
# CONFIG_NFS_FS is not set
# CONFIG_DRM is not set
# CONFIG_GPU_SCHEDULER is not set
# CONFIG_MEDIA_SUPPORT is not set
KEOF

make ARCH=arm64 CROSS_COMPILE=$CROSS olddefconfig

echo "=== Compiling (this takes ~5-10 min) ==="
make ARCH=arm64 CROSS_COMPILE=$CROSS -j$JOBS Image 2>&1 | tail -20

# Copy result
mkdir -p "$OUTPUT"
cp arch/arm64/boot/Image "$OUTPUT/Image"

echo ""
echo "=== Done ==="
file "$OUTPUT/Image"
ls -lh "$OUTPUT/Image"
