#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu
cd "$(dirname "$0")"
. ./Tools/toolchain.sh
mkdir -p build/cache
if [ -n "${LIBUSB_PREFIX:-}" ]; then
    export PKG_CONFIG_PATH="$LIBUSB_PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
elif [ "$(uname -s)" = Darwin ] && command -v brew >/dev/null 2>&1; then
    TASK_USB_PREFIX="$(brew --prefix libusb)"
    export PKG_CONFIG_PATH="$TASK_USB_PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi
command -v pkg-config >/dev/null 2>&1 || { echo 'Install pkg-config (pkgconf is also supported).' >&2; exit 1; }
pkg-config --exists libusb-1.0 || { echo 'Install libusb development files; see README.md.' >&2; exit 1; }
# pkg-config emits compiler/linker option lists; splitting here is intentional.
# Source paths are relative/quoted so a checkout path containing spaces is supported.
"$CC" -O2 -fPIC -std=gnu11 -Wno-switch -Wno-enum-compare -Wno-unused-value -Wno-incompatible-pointer-types-discards-qualifiers $(pkg-config --cflags libusb-1.0) -I Sources/Hardware -c Sources/Hardware/hdtvmate.c -o build/hardware.o
TASK_USB_LIBDIR="$(pkg-config --variable=libdir libusb-1.0)"
"$SWIFTC" -O -swift-version 5 -parse-as-library -module-cache-path build/cache -import-objc-header Sources/Hardware/hdtvmate.h Sources/Capture/*.swift build/hardware.o $(pkg-config --libs libusb-1.0) -lm -lpthread -Xlinker -rpath -Xlinker "$TASK_USB_LIBDIR" -o build/pcapmate
printf 'Built %s (%s/%s)\n' build/pcapmate "$(uname -s)" "$(uname -m)"
