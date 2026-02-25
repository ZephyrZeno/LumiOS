#!/bin/bash
# LumiOS Build Script
# Copyright 2026 Lumi Team. GPLv3
#
# Usage:
#   ./build.sh              # 构建全部组件
#   ./build.sh compositor   # 构建单个组件
#
# 组件:
#   compositor  — Wayland 合成器
#   shell       — 移动端 Shell
#   render      — 渲染抽象层
#   toolkit     — UI 控件工具包
#   lumid       — Init 系统
#   services    — 系统服务 (netd/audiod/sensord/telephonyd/bluetoothd)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${SCRIPT_DIR}/out/build"

COMPOSITOR_DIR="${SCRIPT_DIR}/system/core/lumi-compositor"
SHELL_DIR="${SCRIPT_DIR}/system/core/lumi-shell"
RENDER_DIR="${SCRIPT_DIR}/system/core/lumi-render"
TOOLKIT_DIR="${SCRIPT_DIR}/system/core/lumi-toolkit"

# ── 构建函数 ───────────────────────────────────────────────────

build_meson_component() {
    local name="$1"
    local src_dir="$2"
    local build_dir="${OUT_DIR}/${name}"

    echo ""
    echo "======== Building ${name} ========"
    if [ ! -f "${src_dir}/meson.build" ]; then
        echo "[skip] ${name}: no meson.build found"
        return 0
    fi

    if [ ! -d "${build_dir}" ]; then
        meson setup "${build_dir}" "${src_dir}" --prefix=/usr
    fi
    ninja -C "${build_dir}"
    echo "[done] ${name}"
}

build_gcc_component() {
    local name="$1"
    local src_dir="$2"
    local build_dir="${OUT_DIR}/${name}"
    local output="$3"

    echo ""
    echo "======== Building ${name} ========"
    mkdir -p "${build_dir}"

    local src_files=$(find "${src_dir}/src" -name '*.c' 2>/dev/null)
    if [ -z "${src_files}" ]; then
        echo "[skip] ${name}: no source files"
        return 0
    fi

    local inc=""
    [ -d "${src_dir}/include" ] && inc="-I${src_dir}/include"

    gcc -Wall -Wextra -O2 -std=c11 ${inc} -o "${build_dir}/${output}" ${src_files}
    echo "[done] ${name} → ${build_dir}/${output}"
}

# ── 执行构建 ───────────────────────────────────────────────────

mkdir -p "${OUT_DIR}"

COMPONENT="${1:-all}"

case "${COMPONENT}" in
    compositor)
        build_meson_component "compositor" "${COMPOSITOR_DIR}" ;;
    shell)
        build_meson_component "shell" "${SHELL_DIR}" ;;
    render)
        build_meson_component "render" "${RENDER_DIR}" ;;
    toolkit)
        build_meson_component "toolkit" "${TOOLKIT_DIR}" ;;
    lumid)
        build_gcc_component "lumid" "${SCRIPT_DIR}/system/core/lumid" "lumid" ;;
    netd)
        build_gcc_component "netd" "${SCRIPT_DIR}/system/services/netd" "netd" ;;
    audiod)
        build_gcc_component "audiod" "${SCRIPT_DIR}/system/services/audiod" "audiod" ;;
    all)
        build_meson_component "render"      "${RENDER_DIR}"
        build_meson_component "toolkit"     "${TOOLKIT_DIR}"
        build_meson_component "compositor"  "${COMPOSITOR_DIR}"
        build_meson_component "shell"       "${SHELL_DIR}"
        build_gcc_component   "lumid"       "${SCRIPT_DIR}/system/core/lumid" "lumid"
        build_gcc_component   "sandbox"     "${SCRIPT_DIR}/system/core/sandbox" "lumi-sandbox"
        build_gcc_component   "netd"        "${SCRIPT_DIR}/system/services/netd" "netd"
        build_gcc_component   "audiod"      "${SCRIPT_DIR}/system/services/audiod" "audiod"
        echo ""
        echo "======== Build complete ========"
        ;;
    *)
        echo "Usage: $0 [compositor|shell|render|toolkit|lumid|netd|audiod|all]"
        exit 1 ;;
esac
