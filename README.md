# pcapmate

A tool to get ATSC 3.0 packet captures from an HDTV Mate.

One command-line program and one source tree for **macOS and Linux**, using libusb and Swift. Build the native executable for your operating system, then use the same commands on either platform. No HDHomeRun, Linux VM, Android app or media decoder is required for normal use.

**Validation status:** macOS build, replay/tests and physical Mate capture are verified. Linux portability changes and x86-64/ARM64 CI are included; local end-to-end Linux verification is **pending**, after a test-environment disk failure corrupted the downloaded Swift runtime. Do not treat the Linux hardware path as tested merely because it shares the driver. The GitHub Actions checks run on your first push. Details: [VALIDATION.md](docs/VALIDATION.md).

## Build

Requires Swift **6.2+**, a Clang C compiler and libusb development files. The command is identical on both OSs:

```sh
./build.sh
./test.sh
./Tools/smoke.sh
```

On macOS, install Xcode/Command Line Tools and dependencies:

```sh
brew install libusb pkgconf python3
```

On Ubuntu/Debian:

```sh
sudo apt-get update
sudo apt-get install -y clang libusb-1.0-0-dev pkg-config python3
```

Install the Linux Swift toolchain using [Swift's official instructions](https://www.swift.org/install/linux/). See [Linux setup and USB permissions](docs/LINUX.md). Python is used for the independent inspection/smoke checks, not for normal capture. `CC`, `SWIFTC` and `LIBUSB_PREFIX` can override toolchain/library discovery.

## Capture

```sh
./build/pcapmate --list
mkdir -p captures
./build/pcapmate --rf 16 --plps 0,1 --seconds 30 \
  --firmware /absolute/path/dvb-usb-it9303-01.fw \
  --output captures/rf16-plp0-1.pcap
```

- RF is the US physical broadcast channel, **2–51**. The tool calculates its 6 MHz center frequency.
- PLP IDs are **0–63**, not just 0–3. Select up to **four simultaneously**, for example `--plps 16`, `--plps 0,16` or `--plps 3,17`. Use the actual IDs advertised by your station. A PLP number alone does not identify its payload as BPS or another service.
- `--plps auto` uses the chip's initial automatic selection; it does **not** guarantee every PLP. The standalone CLI does not yet enumerate/decode station L1 Detail itself. See [selection policy and app presets](docs/PLP_SELECTION.md).
- `--seconds` defaults to **30**, accepts **1–3600**, and starts after the first complete ALP record. Acquisition has a 20-second timeout; interval precision follows USB reads, not a hard-real-time clock.
- Optional `--serial SERIAL` chooses a Mate listed by `--list`.
- Supply firmware separately. **No firmware is included or downloaded automatically.** See [FIRMWARE.md](docs/FIRMWARE.md).

Close/release other receiver applications before capture. macOS uses libusb without a kernel extension. Linux may need the supplied narrow udev rule; libusb temporarily detaches an attached kernel driver while this tool owns interface 0 and reattaches it on release.

Output is a classic ALP `.pcap` and a `.pcap.json` report with record/byte counts, requested/observed PLPs and wrapper errors. Existing output files are never overwritten. Errors return a nonzero exit code; a partial PCAP can remain after failure and must not be mistaken for a completed recording. Parent output directories must exist.

## Replay and inspect

Convert an existing **Sony ALP-div-TS USB stream**, not ordinary MPEG-TS:

```sh
./build/pcapmate --replay /path/raw-alp-div-ts.bin --output captures/replayed.pcap
python3 Tools/inspect-pcap.py captures/replayed.pcap
```

Replay preserves ALP bytes/metadata; PCAP timestamps are conversion time because a raw wrapper file has no original USB arrival timestamps.

## What is captured

```
libusb → IT9303 bridge → Sony tuner/demodulator → up to 4 selected PLPs
       → Sony three-byte ALP-div-TS wrapper removal
       → complete ALP records, including F0/F1 metadata
       → classic PCAP: network byte order, nanosecond fields, ALP link type 289
```

The global header matches the supplied HDHomeRun DEV/TECH ALP-PCAP reference: magic `a1b23c4d`, version 2.4, snap length 262144, link type 289. Capture payload naturally differs by station/time. Timestamps are **host USB arrival**, not GPS/RF timestamps; nanosecond field units do not promise 1 ns measurement precision.

F1 records (`08 00 05 f1 00 XX`) attribute the following ALP packet to `XX >> 2`. They are retained from the receiver. If the chip omits repeated F1 markers for a single explicitly selected slot, pcapmate supplies that unambiguous configured ID. Multiple-PLP/automatic captures never guess missing IDs. F0 timing extensions, signaling and other ALP records are preserved. Consumers still parse ALP/IP/UDP/ROUTE/MMTP themselves. This tool does not play media, decrypt protected services or decode BPS messages.

## Source, license and documentation

**GPL-2.0-only**, derived from [Yoonji Park's koreapyj/cxd28xx driver](https://github.com/koreapyj/cxd28xx), pinned to `c5fa27eecbe80633cbb365cc44656daea1c0e2e3`. Original Sony, TurboSight and Yoonji Park notices are retained. The libusb header retains its LGPL-2.1-or-later license; the build dynamically links your installed libusb and distributes no libusb binary. Firmware has separate terms and is excluded.

- [License/dependency audit](docs/LICENSING.md)
- [Native/capture APIs](docs/API.md)
- [PLP IDs and automatic selection](docs/PLP_SELECTION.md)
- [Linux setup](docs/LINUX.md)
- [Validation evidence and limits](docs/VALIDATION.md)
- [First GitHub upload, step by step](docs/GITHUB.md)

`Sources/Hardware` contains the userspace driver; `Sources/Capture` the shared framer, PCAP writer, selection policy and CLI. `upstream/cxd28xx` holds the unmodified source subset needed by `Tools/regenerate.py`; `PROVENANCE.json` records hashes. `Tests` uses synthetic data. The Linux Dockerfile and macOS/Linux GitHub Actions workflow run without a receiver; they do not claim RF hardware testing.

The public source package excludes the parent internal app, encoders/decoders, firmware, vendor APKs, captures and signing credentials. FreeStream and iPad DriverKit capture are separate work.

Development assisted by [Codex](https://github.com/codex); the prepared initial commit includes a co-author trailer. [Acknowledgments](ACKNOWLEDGMENTS.md) retain the upstream driver credits.
