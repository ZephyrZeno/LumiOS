# LumiOS Top-Level Build System / LumiOS 顶层构建系统
# =====================================================
# Usage / 用法:
#   make all DEVICE=generic-arm64    # Full build
#   make kernel                      # Build kernel only
#   make system                      # Build system components
#   make rootfs                      # Generate root filesystem
#   make image                       # Package images
#   make run-vm                      # Start QEMU VM
#   make flash DEVICE=xiaomi-14-pro  # Flash to device

# === Configuration / 配置 ===
DEVICE       ?= generic-arm64
ARCH         ?= aarch64
CROSS_COMPILE ?= aarch64-linux-gnu-
HOST_ARCH    := $(shell uname -m)

# Directories / 目录
TOP          := $(CURDIR)
BUILD_DIR    := $(TOP)/out
KERNEL_DIR   := $(TOP)/kernel
SYSTEM_DIR   := $(TOP)/system
PACKAGES_DIR := $(TOP)/packages
DEVICE_DIR   := $(TOP)/devices/$(DEVICE)
ROOTFS_DIR   := $(BUILD_DIR)/rootfs
IMAGE_DIR    := $(BUILD_DIR)/images
TOOLS_DIR    := $(TOP)/tools
SYSROOT      := $(BUILD_DIR)/sysroot

# Toolchain / 工具链
CC           := $(CROSS_COMPILE)gcc
CXX          := $(CROSS_COMPILE)g++
LD           := $(CROSS_COMPILE)ld
AR           := $(CROSS_COMPILE)ar
STRIP        := $(CROSS_COMPILE)strip
OBJCOPY      := $(CROSS_COMPILE)objcopy

# No cross-compile needed on native aarch64 / 原生 aarch64 无需交叉编译
ifeq ($(HOST_ARCH),aarch64)
    CROSS_COMPILE :=
    CC := gcc
    CXX := g++
    LD := ld
    AR := ar
    STRIP := strip
    OBJCOPY := objcopy
endif

# Kernel version / 内核版本
KERNEL_VERSION := 6.12.8
KERNEL_URL     := https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$(KERNEL_VERSION).tar.xz

# musl libc
MUSL_VERSION   := 1.2.5
MUSL_URL       := https://musl.libc.org/releases/musl-$(MUSL_VERSION).tar.gz

# Common build flags / 通用编译标志
CFLAGS         := -O2 -pipe -march=armv8-a -Wall -Wextra
LDFLAGS        := -L$(SYSROOT)/usr/lib
INCLUDES       := -I$(SYSROOT)/usr/include

# === Top-level targets / 顶层目标 ===
.PHONY: all clean kernel system packages rootfs image run-vm flash help

all: dirs kernel system packages rootfs image
	@echo "=== LumiOS build complete (device: $(DEVICE)) ==="
	@echo "Images at: $(IMAGE_DIR)/"

help:
	@echo "LumiOS Build System"
	@echo "==================="
	@echo ""
	@echo "Targets:"
	@echo "  all          - Full build (kernel+system+packages+rootfs+image)"
	@echo "  kernel       - Build Linux kernel"
	@echo "  system       - Build system components (lumid, compositor, shell)"
	@echo "  packages     - Build package manager"
	@echo "  rootfs       - Generate root filesystem"
	@echo "  image        - Package final images"
	@echo "  run-vm       - Start in QEMU"
	@echo "  flash        - Flash to device"
	@echo "  toolchain    - Download/build cross-compilation toolchain"
	@echo "  clean        - Clean build artifacts"
	@echo ""
	@echo "Variables:"
	@echo "  DEVICE       - Target device (default: generic-arm64)"
	@echo "  ARCH         - Architecture (default: aarch64)"
	@echo "  CROSS_COMPILE- Cross-compile prefix"
	@echo ""
	@echo "Devices:"
	@echo "  generic-arm64  - QEMU virtual machine"
	@echo "  google-pixel   - Google Pixel series"
	@echo "  xiaomi-14-pro  - Xiaomi 14 Pro"

dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(ROOTFS_DIR)
	@mkdir -p $(IMAGE_DIR)
	@mkdir -p $(SYSROOT)/usr/lib
	@mkdir -p $(SYSROOT)/usr/include
	@mkdir -p $(SYSROOT)/usr/bin

# === Toolchain / 工具链 ===
.PHONY: toolchain toolchain-download musl

toolchain: toolchain-download musl
	@echo "=== Toolchain ready ==="

toolchain-download:
	@echo "=== Checking cross-compilation toolchain ==="
	@which $(CC) > /dev/null 2>&1 || \
		(echo "ERROR: $(CC) not found, please install gcc-aarch64-linux-gnu" && exit 1)

musl: dirs
	@echo "=== Building musl libc ==="
	@if [ ! -d "$(BUILD_DIR)/musl-$(MUSL_VERSION)" ]; then \
		echo "Downloading musl $(MUSL_VERSION)..."; \
		cd $(BUILD_DIR) && \
		wget -q $(MUSL_URL) && \
		tar xf musl-$(MUSL_VERSION).tar.gz && \
		rm musl-$(MUSL_VERSION).tar.gz; \
	fi
	@cd $(BUILD_DIR)/musl-$(MUSL_VERSION) && \
		CC="$(CC)" ./configure \
			--prefix=/usr \
			--target=$(ARCH)-linux-musl \
			--disable-shared \
			DESTDIR=$(SYSROOT) && \
		$(MAKE) -j$$(nproc) && \
		$(MAKE) install DESTDIR=$(SYSROOT)

# === Kernel / 内核 ===
kernel: dirs
	@echo "=== Building Linux kernel ($(DEVICE)) ==="
	@if [ ! -d "$(BUILD_DIR)/linux-$(KERNEL_VERSION)" ]; then \
		echo "Downloading kernel source..."; \
		cd $(BUILD_DIR) && \
		wget -q $(KERNEL_URL) && \
		tar xf linux-$(KERNEL_VERSION).tar.xz && \
		rm linux-$(KERNEL_VERSION).tar.xz; \
	fi
	@# Apply device patches / 应用设备补丁
	@if [ -d "$(DEVICE_DIR)/patches/kernel" ]; then \
		for p in $(DEVICE_DIR)/patches/kernel/*.patch; do \
			echo "Applying patch: $$(basename $$p)"; \
			cd $(BUILD_DIR)/linux-$(KERNEL_VERSION) && \
			patch -p1 -N < $$p || true; \
		done; \
	fi
	@# Copy device kernel config / 复制设备内核配置
	@cp $(DEVICE_DIR)/kernel.config \
		$(BUILD_DIR)/linux-$(KERNEL_VERSION)/.config
	@cd $(BUILD_DIR)/linux-$(KERNEL_VERSION) && \
		$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) \
			-j$$(nproc) Image dtbs modules
	@echo "Kernel built: $(BUILD_DIR)/linux-$(KERNEL_VERSION)/arch/arm64/boot/Image"

# === System components / 系统组件 ===
system: system-lumid system-lmpkg system-compositor system-shell
	@echo "=== System components built ==="

system-lumid:
	@echo "=== Building lumid (init system) ==="
	$(MAKE) -C $(SYSTEM_DIR)/core/lumid \
		CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
		BUILD_DIR="$(BUILD_DIR)/lumid" \
		PREFIX=$(SYSROOT)

system-lmpkg:
	@echo "=== Building lmpkg (package manager) ==="
	$(MAKE) -C $(PACKAGES_DIR)/lmpkg \
		CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
		BUILD_DIR="$(BUILD_DIR)/lmpkg" \
		PREFIX=$(SYSROOT)

system-compositor:
	@echo "=== Building lumi-compositor ==="
	cd $(SYSTEM_DIR)/core/lumi-compositor && \
		meson setup $(BUILD_DIR)/lumi-compositor \
			--cross-file $(TOP)/build/toolchain/aarch64-cross.ini \
			--prefix=/usr && \
		ninja -C $(BUILD_DIR)/lumi-compositor && \
		DESTDIR=$(SYSROOT) ninja -C $(BUILD_DIR)/lumi-compositor install

system-shell:
	@echo "=== Building lumi-shell ==="
	cd $(SYSTEM_DIR)/core/lumi-shell && \
		meson setup $(BUILD_DIR)/lumi-shell \
			--cross-file $(TOP)/build/toolchain/aarch64-cross.ini \
			--prefix=/usr && \
		ninja -C $(BUILD_DIR)/lumi-shell && \
		DESTDIR=$(SYSROOT) ninja -C $(BUILD_DIR)/lumi-shell install

# === Package manager / 包管理器 ===
packages: system-lmpkg
	@echo "=== Package system built ==="

# === Root filesystem / 根文件系统 ===
rootfs: system
	@echo "=== Generating root filesystem ==="
	@bash $(TOP)/build/scripts/mkrootfs.sh \
		--sysroot $(SYSROOT) \
		--output $(ROOTFS_DIR) \
		--device $(DEVICE) \
		--device-dir $(DEVICE_DIR)
	@echo "rootfs complete: $(ROOTFS_DIR)/"

# === Image packaging / 镜像打包 ===
image: rootfs kernel
	@echo "=== Packaging system images ==="
	@bash $(TOP)/build/scripts/mkimage.sh \
		--rootfs $(ROOTFS_DIR) \
		--kernel $(BUILD_DIR)/linux-$(KERNEL_VERSION)/arch/arm64/boot/Image \
		--output $(IMAGE_DIR) \
		--device $(DEVICE) \
		--device-dir $(DEVICE_DIR)
	@echo "Images complete: $(IMAGE_DIR)/"

# === QEMU VM / QEMU 虚拟机 ===
run-vm: $(IMAGE_DIR)/lumios-generic-arm64.img
	@echo "=== Starting QEMU VM ==="
	qemu-system-aarch64 \
		-machine virt,gic-version=3 \
		-cpu cortex-a76 \
		-smp 4 \
		-m 4G \
		-nographic \
		-kernel $(BUILD_DIR)/linux-$(KERNEL_VERSION)/arch/arm64/boot/Image \
		-drive file=$(IMAGE_DIR)/lumios-generic-arm64.img,format=raw,if=virtio \
		-append "root=/dev/vda2 rw console=ttyAMA0 init=/sbin/lumid" \
		-device virtio-net-pci,netdev=net0 \
		-netdev user,id=net0,hostfwd=tcp::2222-:22 \
		-device virtio-gpu-pci \
		-device virtio-keyboard-pci \
		-device virtio-tablet-pci

# QEMU with GUI / 带图形界面的 QEMU
run-vm-gui:
	@echo "=== Starting QEMU VM (GUI) ==="
	qemu-system-aarch64 \
		-machine virt,gic-version=3 \
		-cpu cortex-a76 \
		-smp 4 \
		-m 4G \
		-kernel $(BUILD_DIR)/linux-$(KERNEL_VERSION)/arch/arm64/boot/Image \
		-drive file=$(IMAGE_DIR)/lumios-generic-arm64.img,format=raw,if=virtio \
		-append "root=/dev/vda2 rw console=tty0 init=/sbin/lumid" \
		-device virtio-gpu-pci,xres=1080,yres=2400 \
		-device virtio-keyboard-pci \
		-device virtio-tablet-pci \
		-device virtio-net-pci,netdev=net0 \
		-netdev user,id=net0 \
		-display sdl,gl=on

# === Flash / 刷写 ===
flash:
	@echo "=== Flashing to device: $(DEVICE) ==="
	@if [ ! -f "$(DEVICE_DIR)/flash.sh" ]; then \
		echo "ERROR: device $(DEVICE) does not support flashing"; \
		exit 1; \
	fi
	@bash $(DEVICE_DIR)/flash.sh $(IMAGE_DIR)

# === Clean / 清理 ===
clean:
	@echo "=== Cleaning build artifacts ==="
	rm -rf $(BUILD_DIR)

clean-kernel:
	rm -rf $(BUILD_DIR)/linux-$(KERNEL_VERSION)

clean-system:
	rm -rf $(BUILD_DIR)/lumid
	rm -rf $(BUILD_DIR)/lmpkg
	rm -rf $(BUILD_DIR)/lumi-compositor
	rm -rf $(BUILD_DIR)/lumi-shell

# === Development tools / 开发工具 ===
.PHONY: format lint test

format:
	find $(SYSTEM_DIR) $(PACKAGES_DIR) -name '*.c' -o -name '*.h' | \
		xargs clang-format -i --style=file

lint:
	find $(SYSTEM_DIR) $(PACKAGES_DIR) -name '*.c' | \
		xargs cppcheck --enable=all --suppress=missingIncludeSystem

test:
	$(MAKE) -C $(SYSTEM_DIR)/core/lumid test
	$(MAKE) -C $(PACKAGES_DIR)/lmpkg test
