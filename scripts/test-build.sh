#!/bin/bash
# test-build.sh - LumiOS Build Verification Script / 构建验证脚本
#
# Run this in WSL2 to verify all components compile correctly.
# 在 WSL2 中运行此脚本验证所有组件编译正确。
#
# Usage / 用法:
#   ./scripts/test-build.sh           # Full test (all modules)
#   ./scripts/test-build.sh lumid     # Test only lumid
#   ./scripts/test-build.sh lmpkg     # Test only lmpkg
#   ./scripts/test-build.sh deps      # Install dependencies only
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0

TOP="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$TOP/out/test"
mkdir -p "$BUILD"

log_pass() { echo -e "  ${GREEN}PASS${NC} $1"; PASS=$((PASS+1)); }
log_fail() { echo -e "  ${RED}FAIL${NC} $1: $2"; FAIL=$((FAIL+1)); }
log_skip() { echo -e "  ${YELLOW}SKIP${NC} $1: $2"; SKIP=$((SKIP+1)); }
log_step() { echo -e "\n${BLUE}=== $1 ===${NC}"; }

# === Step 0: Check/install dependencies / 检查/安装依赖 ===

install_deps() {
    log_step "Installing build dependencies"

    if ! command -v apt &>/dev/null; then
        echo "ERROR: apt not found. This script requires Ubuntu/Debian WSL2."
        exit 1
    fi

    sudo apt update -qq
    sudo apt install -y -qq \
        build-essential \
        gcc \
        pkg-config \
        libarchive-dev \
        libzstd-dev \
        2>&1 | tail -5

    # Optional: cross-compiler and Wayland deps
    # 可选：交叉编译器和 Wayland 依赖
    echo ""
    echo "Optional packages (for full build):"
    echo "  sudo apt install gcc-aarch64-linux-gnu"
    echo "  sudo apt install libwayland-dev libwlroots-dev libinput-dev"
    echo "  sudo apt install libcairo2-dev libpango1.0-dev libglib2.0-dev"
    echo "  sudo apt install meson ninja-build qemu-system-aarch64"

    log_pass "dependencies installed"
}

# === Step 1: Test lumid compilation / 测试 lumid 编译 ===

test_lumid() {
    log_step "Testing lumid (init system) compilation"

    local SRC="$TOP/system/core/lumid"
    local OUT="$BUILD/lumid"
    mkdir -p "$OUT"

    CC="${CC:-gcc}"
    CFLAGS="-O2 -Wall -Wextra -std=gnu11 -Wno-unused-parameter -Wno-unused-function"

    local sources=(
        src/main.c src/service.c src/supervisor.c src/config.c
        src/cgroup.c src/socket.c src/mount.c src/log.c src/util.c
    )

    # Compile each .c file individually / 逐个编译 .c 文件
    local compiled=0
    for src in "${sources[@]}"; do
        local base=$(basename "$src" .c)
        if $CC $CFLAGS -I"$SRC/include" -c "$SRC/$src" -o "$OUT/$base.o" 2>"$OUT/$base.err"; then
            log_pass "lumid/$src"
            compiled=$((compiled+1))
        else
            log_fail "lumid/$src" "$(head -3 "$OUT/$base.err")"
        fi
    done

    # Link (will fail without Linux headers on non-Linux, that's OK for WSL)
    # 链接（非 Linux 上没有 Linux 头文件会失败，WSL 上应该可以）
    if [ $compiled -eq ${#sources[@]} ]; then
        local objs=""
        for src in "${sources[@]}"; do
            objs="$objs $OUT/$(basename "$src" .c).o"
        done
        if $CC $CFLAGS -o "$OUT/lumid" $objs 2>"$OUT/link.err"; then
            log_pass "lumid link -> $OUT/lumid"
            echo "    Size: $(du -h "$OUT/lumid" | cut -f1)"
        else
            log_fail "lumid link" "$(head -3 "$OUT/link.err")"
        fi
    fi

    # Also compile lumictl / 也编译 lumictl
    local ctl_sources=(src/lumictl.c src/socket.c src/util.c src/log.c)
    local ctl_compiled=0
    for src in "${ctl_sources[@]}"; do
        local base="ctl_$(basename "$src" .c)"
        if $CC $CFLAGS -I"$SRC/include" -DLUMICTL_BUILD -c "$SRC/$src" -o "$OUT/$base.o" 2>"$OUT/$base.err"; then
            ctl_compiled=$((ctl_compiled+1))
        fi
    done
    if [ $ctl_compiled -eq ${#ctl_sources[@]} ]; then
        local ctl_objs=""
        for src in "${ctl_sources[@]}"; do
            ctl_objs="$ctl_objs $OUT/ctl_$(basename "$src" .c).o"
        done
        if $CC $CFLAGS -o "$OUT/lumictl" $ctl_objs 2>/dev/null; then
            log_pass "lumictl link -> $OUT/lumictl"
        fi
    fi
}

# === Step 2: Test lmpkg compilation / 测试 lmpkg 编译 ===

test_lmpkg() {
    log_step "Testing lmpkg (package manager) compilation"

    local SRC="$TOP/packages/lmpkg"
    local OUT="$BUILD/lmpkg"
    mkdir -p "$OUT"

    CC="${CC:-gcc}"
    CFLAGS="-O2 -Wall -Wextra -std=gnu11 -Wno-unused-parameter -Wno-unused-function"

    # Check for libarchive / 检查 libarchive
    if ! pkg-config --exists libarchive 2>/dev/null; then
        log_skip "lmpkg" "libarchive not installed (sudo apt install libarchive-dev)"
        return
    fi

    local ARCHIVE_CFLAGS=$(pkg-config --cflags libarchive 2>/dev/null)
    local ARCHIVE_LIBS=$(pkg-config --libs libarchive 2>/dev/null)

    local sources=(
        src/main.c src/database.c src/package.c src/install.c
        src/remove.c src/sync.c src/depsolve.c src/version.c src/util.c
    )

    local compiled=0
    for src in "${sources[@]}"; do
        local base=$(basename "$src" .c)
        if $CC $CFLAGS $ARCHIVE_CFLAGS -I"$SRC/include" -c "$SRC/$src" -o "$OUT/$base.o" 2>"$OUT/$base.err"; then
            log_pass "lmpkg/$src"
            compiled=$((compiled+1))
        else
            log_fail "lmpkg/$src" "$(head -3 "$OUT/$base.err")"
        fi
    done

    # Link / 链接
    if [ $compiled -eq ${#sources[@]} ]; then
        local objs=""
        for src in "${sources[@]}"; do
            objs="$objs $OUT/$(basename "$src" .c).o"
        done
        if $CC $CFLAGS -o "$OUT/lmpkg" $objs $ARCHIVE_LIBS -lzstd -lcrypto 2>"$OUT/link.err"; then
            log_pass "lmpkg link -> $OUT/lmpkg"
            echo "    Size: $(du -h "$OUT/lmpkg" | cut -f1)"
        else
            log_fail "lmpkg link" "$(head -3 "$OUT/link.err")"
        fi
    fi

    # Run version unit tests / 运行版本单元测试
    log_step "Running lmpkg unit tests"
    if $CC $CFLAGS -I"$SRC/include" -DLMPKG_TEST "$SRC/src/version.c" -o "$OUT/test_version" 2>"$OUT/test_ver.err"; then
        if "$OUT/test_version" 2>&1; then
            log_pass "lmpkg version tests"
        else
            log_fail "lmpkg version tests" "test execution failed"
        fi
    else
        log_fail "lmpkg version test compile" "$(head -3 "$OUT/test_ver.err")"
    fi
}

# === Step 3: Test android-container compilation / 测试 Android 容器编译 ===

test_android() {
    log_step "Testing android-container compilation"

    local SRC="$TOP/android-compat/container"
    local OUT="$BUILD/android"
    mkdir -p "$OUT"

    CC="${CC:-gcc}"
    CFLAGS="-O2 -Wall -Wextra -std=gnu11 -Wno-unused-parameter -Wno-unused-function"

    local sources=(
        src/main.c src/container.c src/namespace.c src/mounts.c
        src/cgroup.c src/properties.c
    )

    local compiled=0
    for src in "${sources[@]}"; do
        local base=$(basename "$src" .c)
        if $CC $CFLAGS -I"$SRC/include" -c "$SRC/$src" -o "$OUT/$base.o" 2>"$OUT/$base.err"; then
            log_pass "android/$src"
            compiled=$((compiled+1))
        else
            log_fail "android/$src" "$(head -3 "$OUT/$base.err")"
        fi
    done

    if [ $compiled -eq ${#sources[@]} ]; then
        local objs=""
        for src in "${sources[@]}"; do
            objs="$objs $OUT/$(basename "$src" .c).o"
        done
        if $CC $CFLAGS -o "$OUT/android-container" $objs 2>"$OUT/link.err"; then
            log_pass "android-container link -> $OUT/android-container"
        else
            log_fail "android-container link" "$(head -3 "$OUT/link.err")"
        fi
    fi
}

# === Step 4: Test compositor compilation (needs wlroots) / 测试合成器编译 ===

test_compositor() {
    log_step "Testing lumi-compositor compilation (header check)"

    local SRC="$TOP/system/core/lumi-compositor"

    # Just check if we can parse the header without wlroots
    # 只检查不依赖 wlroots 是否能解析头文件
    if pkg-config --exists wlroots 2>/dev/null; then
        echo "  wlroots found, attempting full compile..."
        cd "$SRC"
        if meson setup "$BUILD/compositor" --prefix=/usr 2>"$BUILD/compositor_setup.err"; then
            if ninja -C "$BUILD/compositor" 2>"$BUILD/compositor_build.err"; then
                log_pass "lumi-compositor full build"
            else
                log_fail "lumi-compositor build" "$(tail -5 "$BUILD/compositor_build.err")"
            fi
        else
            log_fail "lumi-compositor meson setup" "$(tail -3 "$BUILD/compositor_setup.err")"
        fi
    else
        log_skip "lumi-compositor" "wlroots not installed (sudo apt install libwlroots-dev)"
    fi
}

# === Step 5: Test shell compilation (needs cairo/pango) / 测试 Shell 编译 ===

test_shell() {
    log_step "Testing lumi-shell compilation (header check)"

    local SRC="$TOP/system/core/lumi-shell"

    if pkg-config --exists cairo pangocairo glib-2.0 2>/dev/null; then
        echo "  cairo/pango found, attempting full compile..."
        cd "$SRC"
        if meson setup "$BUILD/shell" --prefix=/usr 2>"$BUILD/shell_setup.err"; then
            if ninja -C "$BUILD/shell" 2>"$BUILD/shell_build.err"; then
                log_pass "lumi-shell full build"
            else
                log_fail "lumi-shell build" "$(tail -5 "$BUILD/shell_build.err")"
            fi
        else
            log_fail "lumi-shell meson setup" "$(tail -3 "$BUILD/shell_setup.err")"
        fi
    else
        log_skip "lumi-shell" "cairo/pango not installed (sudo apt install libcairo2-dev libpango1.0-dev)"
    fi
}

# === Summary / 汇总 ===

print_summary() {
    echo ""
    echo "========================================="
    echo -e "  ${GREEN}PASS: $PASS${NC}  ${RED}FAIL: $FAIL${NC}  ${YELLOW}SKIP: $SKIP${NC}"
    echo "========================================="
    echo "  Build output: $BUILD/"
    echo ""

    if [ $FAIL -gt 0 ]; then
        echo -e "${RED}Some tests failed. Check error logs in $BUILD/${NC}"
        echo "  Error files: find $BUILD -name '*.err' -size +0"
        return 1
    elif [ $PASS -gt 0 ]; then
        echo -e "${GREEN}All compiled tests passed!${NC}"

        echo ""
        echo "Next steps:"
        echo "  1. Install optional deps for full build:"
        echo "     sudo apt install libwlroots-dev libcairo2-dev libpango1.0-dev libglib2.0-dev meson ninja-build"
        echo "  2. Re-run this script for compositor and shell tests"
        echo "  3. Install QEMU for VM testing:"
        echo "     sudo apt install qemu-system-aarch64"
        return 0
    fi
}

# === Main / 主入口 ===

echo "LumiOS Build Verification"
echo "========================="
echo "Project: $TOP"
echo "Output:  $BUILD"
echo "Date:    $(date)"

case "${1:-all}" in
    deps)
        install_deps
        ;;
    lumid)
        test_lumid
        print_summary
        ;;
    lmpkg)
        test_lmpkg
        print_summary
        ;;
    android)
        test_android
        print_summary
        ;;
    compositor)
        test_compositor
        print_summary
        ;;
    shell)
        test_shell
        print_summary
        ;;
    all)
        test_lumid
        test_lmpkg
        test_android
        test_compositor
        test_shell
        print_summary
        ;;
    *)
        echo "Usage: $0 {all|deps|lumid|lmpkg|android|compositor|shell}"
        exit 1
        ;;
esac
