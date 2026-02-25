#!/bin/bash
# Extract raw ARM64 Image from EFI PE vmlinuz
set -e
cd /mnt/z/lacrus-projects/LumiOS/out/vm-build/kernel

INPUT="Image"
[ -f "Image.efi" ] && INPUT="Image.efi"

echo "Input: $INPUT ($(file $INPUT))"

# Find gzip magic bytes (1f 8b 08) offset in the PE file
OFFSET=$(od -A d -t x1 "$INPUT" | grep -m1 '1f 8b 08' | awk '{print $1}')

if [ -z "$OFFSET" ]; then
    # Try python fallback
    OFFSET=$(python3 -c "
import sys
data = open('$INPUT','rb').read()
idx = data.find(b'\x1f\x8b\x08')
print(idx if idx >= 0 else '')
" 2>/dev/null)
fi

if [ -n "$OFFSET" ] && [ "$OFFSET" != "-1" ] && [ "$OFFSET" != "" ]; then
    echo "Found gzip at offset: $OFFSET"
    dd if="$INPUT" bs=1 skip="$OFFSET" 2>/dev/null | gunzip > Image.raw 2>/dev/null
    if [ -s Image.raw ]; then
        mv Image.raw Image
        echo "Extracted: $(file Image)"
        echo "Size: $(ls -lh Image | awk '{print $5}')"
        exit 0
    fi
fi

echo "Could not extract raw kernel from EFI image."
echo "The kernel needs to be a raw ARM64 Image, not EFI PE format."
echo ""
echo "Please download from this direct URL instead:"
echo "  https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/aarch64/netboot/vmlinuz-lts"
echo ""
echo "Then run: gunzip -S '' vmlinuz-lts  (if it's gzip compressed)"
exit 1
