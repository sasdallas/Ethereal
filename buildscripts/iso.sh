#!/bin/bash

# iso.sh - Constructs an ISO image

set -e

if [ "$#" -eq 0 ]; then
    BUILDSCRIPTS_ROOT=${BUILDSCRIPTS_ROOT:-"$(cd "$(dirname "$0")" && pwd)"}
    PROJECT_ROOT=${PROJECT_ROOT:-"${BUILDSCRIPTS_ROOT}/.."}
    . "$BUILDSCRIPTS_ROOT/config.sh"
    set -- "$ISO_OUTPUT_DIRECTORY/hexahedron.iso" "$SYSROOT/$BOOT_DIRECTORY" "$PROJECT_ROOT/conf"
elif [ "$#" -ne 3 ]; then
    echo "Usage: $0 [iso-file boot-directory config-directory]" >&2
    exit 2
fi

ISO_FILE=$1
BOOT_DIR=$2
CONF_DIR=$3
ISO_DIR=$(mktemp -d "$(dirname "$ISO_FILE")/.ethereal-iso.XXXXXX")
trap 'rm -r -- "$ISO_DIR"' EXIT

# Create ISO directories
mkdir -p "$ISO_DIR/boot/grub/fonts/" "$ISO_DIR/boot/grub/themes/ethereal/"
cp "$BOOT_DIR/hexahedron-kernel.elf" "$ISO_DIR/boot/"
cp "$BOOT_DIR/initrd.tar.img" "$ISO_DIR/boot/"
if [ -f "$BOOT_DIR/initrd_tiny.tar.img" ]; then
    cp "$BOOT_DIR/initrd_tiny.tar.img" "$ISO_DIR/boot/"
fi

cp -r "$CONF_DIR/theme/." "$ISO_DIR/boot/grub/themes/ethereal/"
cp "$CONF_DIR/grub.cfg" "$ISO_DIR/boot/grub/"
if [ -d "$CONF_DIR/extra-boot-files" ]; then
    cp -r "$CONF_DIR/extra-boot-files/." "$ISO_DIR/boot/"
fi

mv "$ISO_DIR/boot/grub/themes/ethereal/"*.pf2 "$ISO_DIR/boot/grub/fonts/"

# Pack it into an ISO
grub-mkrescue -o "$ISO_FILE" "$ISO_DIR" --themes=starfield
