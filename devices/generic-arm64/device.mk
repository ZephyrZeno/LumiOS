# generic-arm64 - QEMU Virtual Machine Target / QEMU 虚拟机目标
# ==============================================================
#
# Level 0 device for development and testing.
# 用于开发和测试的 Level 0 设备。

DEVICE_NAME     := generic-arm64
DEVICE_DESC     := QEMU ARM64 Virtual Machine
DEVICE_ARCH     := aarch64
DEVICE_KERNEL   := mainline
DEVICE_DTB      := virt.dtb

# Display / 显示
DISPLAY_WIDTH   := 1080
DISPLAY_HEIGHT  := 2400
DISPLAY_DPI     := 440
DISPLAY_REFRESH := 60

# Hardware features / 硬件特性
HAS_GPU         := virtio-gpu
HAS_TOUCH       := virtio-tablet
HAS_KEYBOARD    := virtio-keyboard
HAS_NETWORK     := virtio-net
HAS_STORAGE     := virtio-blk
HAS_AUDIO       := no
HAS_CAMERA      := no
HAS_BLUETOOTH   := no
HAS_SENSORS     := no
HAS_MODEM       := no

# Firmware / 固件
FIRMWARE_FILES  :=

# Partitions / 分区
BOOT_SIZE       := 64M
SYSTEM_SIZE     := 2G
DATA_SIZE       := 4G
