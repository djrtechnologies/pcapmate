# SPDX-License-Identifier: GPL-2.0-only
# Shared compiler discovery; sourced by build.sh and test.sh.
case "$(uname -s)" in
    Darwin)
        if [ -z "${DEVELOPER_DIR:-}" ] && [ -d /Applications/Xcode.app/Contents/Developer ]; then
            export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
        fi
        export SDKROOT="${SDKROOT:-$(xcrun --sdk macosx --show-sdk-path)}"
        CC="${CC:-$(xcrun --sdk macosx --find clang)}"
        SWIFTC="${SWIFTC:-$(xcrun --sdk macosx --find swiftc)}"
        ;;
    Linux)
        CC="${CC:-clang}"
        SWIFTC="${SWIFTC:-swiftc}"
        ;;
    *) echo "Supported platforms are macOS and Linux." >&2; exit 1 ;;
esac
command -v "$CC" >/dev/null 2>&1 || { echo "C compiler not found: $CC" >&2; exit 1; }
command -v "$SWIFTC" >/dev/null 2>&1 || { echo "Install Swift 6.2 or newer; compiler not found: $SWIFTC" >&2; exit 1; }
