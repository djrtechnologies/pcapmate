# License and provenance

## Project license

The combined native driver and capture executable are **GPL-2.0-only**, matching the upstream hardware driver. Keep the full `LICENSE` and original copyright notices when sharing source or binaries. Do not relabel the combined tool BSD, MIT or Apache. Source modifications and the new capture/framing/filesystem/CLI code are supplied under GPL-2.0-only as well.

Upstream: https://github.com/koreapyj/cxd28xx

Pinned revision: `c5fa27eecbe80633cbb365cc44656daea1c0e2e3`.

Upstream `cxd2878.c` credits:

- Sony Semiconductor Solutions Corporation, 2022.
- Davin Zhang / TurboSight, 2021.
- Yoonji Park, 2026.

Upstream `it930x.c` credits Yoonji Park, 2026. Its module license and repository license are GPL. The original header's `SPX-License-Identifier` spelling is retained in the unmodified snapshot; the project license does not depend on correcting that typo.

The userspace adaptation and capture integration were developed for Dylan Ross's internal tools in November 2026. Modified native files and generated subsets retain the upstream notices; new project files carry an SPDX identifier. `PROVENANCE.json` records source hashes. `Tools/regenerate.py` reproduces `bridge.inc`, `demod.inc` and the two adapted headers.

## Adaptation

`compat.h` supplies narrow Linux/DVB/firmware/pthread/libusb adapters. The generator removes kernel entry points and frontend registration that this userspace program does not use. It fixes the ATSC command-result precedence expressions and initializes a reset wait counter. `hdtvmate.c` adds macOS/Linux enumeration, initialization, explicit ATSC/VSB selection, validated up-to-four-PLP configuration, chunked monitor reads, USB reads and cleanup. `HDTVMateALP.swift` removes Sony's three-byte wrappers and preserves ALP records. The PCAP writer, CLI and shared station-derived preset/automatic-ranking helper are new code. The November 16 preset update does not change the project/dependency licenses. The complete buildable subset is included; no private vendor SDK is needed.

## libusb

The copied public libusb header retains its original **LGPL-2.1-or-later** notices. That license text is in `LICENSES/LGPL-2.1-or-later.txt`. The build uses an external, dynamically linked libusb library installed through Homebrew on macOS or the Linux distribution’s package manager and ships no libusb binary. If distributing libusb binaries yourself, include the applicable LGPL notices and corresponding source/other required materials for that version. Use the official license as the authority: https://github.com/libusb/libusb/blob/master/COPYING .

## Firmware and captures

Firmware is a separate device program, not automatically GPL because the host driver is GPL. No firmware blob, vendor APK, decrypted app or media capture is included in this repository. Users supply firmware they are entitled to use. Consult the actual firmware's redistribution terms before adding any blob to a public repository or binary package.

The supplied HDHomeRun reference and local RF validation captures contain third-party broadcast content and are excluded from the source release. Tests use synthetic data. Apple SDK / system frameworks are external build/runtime requirements, not copied source in this project.

Publishing this standalone subset does not publish or relicense the whole internal app. If distributing the internal app or other binaries linked with this GPL driver, assess that complete distribution separately; the fact that the standalone subset is public does not by itself satisfy obligations for other combined works. Primary GPL terms: https://www.gnu.org/licenses/old-licenses/gpl-2.0.html .

## macOS/Linux portability update — 16 November 2026

The shared userspace backend now uses POSIX `clock_gettime(CLOCK_MONOTONIC)`, tracks interface claims, and enables libusb automatic kernel-driver detach/reattach on Linux. Build/test scripts discover Clang, Swift and libusb using the local platform and pkg-config. The Swift code explicitly imports Darwin or Glibc for POSIX calls. These are project modifications under the existing GPL-2.0-only license; the unmodified upstream snapshot and its notices remain intact.
