#!/bin/bash
# build-skia.sh - Download and build Skia as a static library for LumiOS
# 下载并编译 Skia 静态库
#
# Usage: ./scripts/build-skia.sh [--arch x86_64|aarch64]
# Output: third_party/skia/out/{arch}/libskia.a + headers
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
SKIA_DIR="$ROOT_DIR/third_party/skia"
ARCH="${1:-x86_64}"

# Parse --arch flag
while [[ $# -gt 0 ]]; do
    case $1 in
        --arch) ARCH="$2"; shift 2 ;;
        *) shift ;;
    esac
done

echo "[build-skia] Target architecture: $ARCH"
echo "[build-skia] Skia directory: $SKIA_DIR"

# Step 1: Install build dependencies
echo "[build-skia] Checking dependencies..."
if ! command -v python3 &>/dev/null; then
    echo "ERROR: python3 is required"; exit 1
fi
if ! command -v git &>/dev/null; then
    echo "ERROR: git is required"; exit 1
fi
if ! command -v ninja &>/dev/null; then
    echo "ERROR: ninja is required"; exit 1
fi

# Step 2: Clone depot_tools if not present
DEPOT_TOOLS="$ROOT_DIR/third_party/depot_tools"
if [ ! -d "$DEPOT_TOOLS" ]; then
    echo "[build-skia] Cloning depot_tools..."
    git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS"
fi
export PATH="$DEPOT_TOOLS:$PATH"

# Step 3: Fetch Skia source
if [ ! -d "$SKIA_DIR" ]; then
    echo "[build-skia] Cloning Skia source (m131 branch)..."
    mkdir -p "$ROOT_DIR/third_party"
    git clone --depth 1 --branch chrome/m131 https://skia.googlesource.com/skia.git "$SKIA_DIR"
    cd "$SKIA_DIR"
    echo "[build-skia] Syncing Skia dependencies..."
    python3 tools/git-sync-deps
else
    echo "[build-skia] Skia source already exists at $SKIA_DIR"
    cd "$SKIA_DIR"
fi

# Step 4: Configure build with GN
OUT_DIR="$SKIA_DIR/out/$ARCH"
mkdir -p "$OUT_DIR"

echo "[build-skia] Configuring Skia for $ARCH..."

if [ "$ARCH" = "aarch64" ]; then
    # Cross-compile for aarch64
    cat > "$OUT_DIR/args.gn" << 'EOF'
# LumiOS Skia build configuration (aarch64 cross-compile)
is_official_build = true
is_debug = false
target_cpu = "arm64"
target_os = "linux"

# Static library output
is_component_build = false

# Enable GPU backends
skia_use_gl = true
skia_use_vulkan = true
skia_use_egl = true

# Enable image codecs
skia_use_libjpeg_turbo_decode = true
skia_use_libjpeg_turbo_encode = false
skia_use_libpng_decode = true
skia_use_libpng_encode = true
skia_use_libwebp_decode = true
skia_use_libwebp_encode = false

# Text rendering
skia_use_freetype = true
skia_use_fontconfig = true
skia_use_harfbuzz = true
skia_use_icu = false
skia_use_system_freetype2 = false
skia_use_system_harfbuzz = false

# Disable unused features to reduce binary size
skia_enable_pdf = false
skia_enable_skottie = false
skia_enable_tools = false
skia_enable_gpu_debug_layers = false
skia_use_dng_sdk = false
skia_use_piex = false
skia_use_wuffs = true

# Cross-compile toolchain
cc = "aarch64-linux-gnu-gcc"
cxx = "aarch64-linux-gnu-g++"
ar = "aarch64-linux-gnu-ar"
EOF
else
    # Native x86_64 build
    cat > "$OUT_DIR/args.gn" << 'EOF'
# LumiOS Skia build configuration (x86_64 native)
is_official_build = true
is_debug = false
target_cpu = "x64"

# Static library output
is_component_build = false

# Enable GPU backends
skia_use_gl = true
skia_use_vulkan = true
skia_use_egl = true

# Enable image codecs
skia_use_libjpeg_turbo_decode = true
skia_use_libjpeg_turbo_encode = false
skia_use_libpng_decode = true
skia_use_libpng_encode = true
skia_use_libwebp_decode = true
skia_use_libwebp_encode = false

# Text rendering
skia_use_freetype = true
skia_use_fontconfig = true
skia_use_harfbuzz = true
skia_use_icu = false
skia_use_system_freetype2 = true
skia_use_system_harfbuzz = false

# Disable unused features
skia_enable_pdf = false
skia_enable_skottie = false
skia_enable_tools = false
skia_enable_gpu_debug_layers = false
skia_use_dng_sdk = false
skia_use_piex = false
skia_use_wuffs = true
EOF
fi

# Step 5: Generate ninja files
cd "$SKIA_DIR"
bin/gn gen "$OUT_DIR"

# Step 6: Build
echo "[build-skia] Building Skia (this may take 10-30 minutes)..."
ninja -C "$OUT_DIR" skia

# Step 7: Verify output
if [ -f "$OUT_DIR/libskia.a" ]; then
    SIZE=$(du -h "$OUT_DIR/libskia.a" | cut -f1)
    echo "[build-skia] SUCCESS: $OUT_DIR/libskia.a ($SIZE)"
else
    echo "[build-skia] ERROR: libskia.a not found!"
    exit 1
fi

# Step 8: Create pkg-config file for meson
PKG_DIR="$OUT_DIR/pkgconfig"
mkdir -p "$PKG_DIR"
cat > "$PKG_DIR/skia.pc" << EOF
prefix=$SKIA_DIR
libdir=$OUT_DIR
includedir=$SKIA_DIR

Name: skia
Description: Skia 2D Graphics Library for LumiOS
Version: 131.0
Libs: -L\${libdir} -lskia -lstdc++ -lm -ldl -lpthread -lfreetype -lfontconfig -lGL -lEGL
Cflags: -I\${includedir} -I\${includedir}/include
EOF

echo "[build-skia] pkg-config file: $PKG_DIR/skia.pc"
echo "[build-skia] To use: export PKG_CONFIG_PATH=$PKG_DIR:\$PKG_CONFIG_PATH"
echo "[build-skia] Done!"
