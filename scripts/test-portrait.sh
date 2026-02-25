#!/bin/bash
# test-portrait.sh - Launch LumiOS in portrait mode on X11
# 在 X11 上以竖屏模式启动 LumiOS
#
# Usage: ./scripts/test-portrait.sh [WxH]
# Default: 450x900
#
set -e
cd "$(dirname "$0")/.."

W="${1:-450}"
H="${2:-900}"
if [[ "$1" == *x* ]]; then
    W="${1%%x*}"
    H="${1##*x}"
fi

echo "[test-portrait] Resolution: ${W}x${H}"

# Add custom modeline via xrandr
MODELINE=$(cvt "$W" "$H" 60 2>/dev/null | grep Modeline | sed 's/Modeline //')
MODE_NAME="${W}x${H}_60.00"

if [ -n "$MODELINE" ]; then
    echo "[test-portrait] Adding xrandr mode: $MODE_NAME"
    xrandr --newmode $MODELINE 2>/dev/null || true

    # Find the current output name (e.g., Virtual-1, eDP-1, HDMI-1)
    OUTPUT=$(xrandr --query | grep ' connected' | head -1 | awk '{print $1}')
    echo "[test-portrait] Output: $OUTPUT"
    xrandr --addmode "$OUTPUT" "$MODE_NAME" 2>/dev/null || true
fi

# Kill old instances
killall lumi-compositor lumi-shell 2>/dev/null || true
rm -f /run/user/$(id -u)/wayland-lumi.lock
sleep 0.5

# Start compositor with portrait resolution
echo "[test-portrait] Starting compositor..."
WLR_BACKENDS=x11 \
WLR_RENDERER=pixman \
LIBGL_ALWAYS_SOFTWARE=1 \
./out/test/compositor/lumi-compositor -s wayland-lumi -r "${W}x${H}" 2>&1 &
COMP_PID=$!
sleep 2

# Check if compositor is still running
if ! kill -0 $COMP_PID 2>/dev/null; then
    echo "[test-portrait] Compositor failed with -r ${W}x${H}"
    echo "[test-portrait] Trying without -r flag (default resolution)..."
    WLR_BACKENDS=x11 \
    WLR_RENDERER=pixman \
    LIBGL_ALWAYS_SOFTWARE=1 \
    ./out/test/compositor/lumi-compositor -s wayland-lumi 2>&1 &
    COMP_PID=$!
    sleep 2
fi

# Start shell
echo "[test-portrait] Starting shell..."
WAYLAND_DISPLAY=wayland-lumi ./out/test/shell/lumi-shell 2>&1

# Cleanup
kill $COMP_PID 2>/dev/null || true
