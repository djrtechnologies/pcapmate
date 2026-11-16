#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu
cd "$(dirname "$0")"
. ./Tools/toolchain.sh
mkdir -p build/cache
"$SWIFTC" -swift-version 5 -parse-as-library -module-cache-path build/cache Sources/Capture/HDTVMateALP.swift Sources/Capture/ALPPCAPFileWriter.swift Sources/Capture/HDTVMatePLPSelection.swift Tests/main.swift -o build/capture-tests
build/capture-tests
