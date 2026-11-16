# Validation — 15 November 2026

Hardware: GTMedia / Zenview HDTV Mate, USB `23e2:2b02`, serial ending `00001`, antenna connected, macOS Apple silicon. Generic IT9303 bridge firmware 1.4.0.0; exact hash in FIRMWARE.md.

## Live RF16

- Explicit simultaneous PLPs **0,1**, requested duration 30 seconds: **126,461 PCAP records**, **93,163,148 bytes**, first-to-last timestamp span **30.020493 seconds** (USB read granularity), zero transport-error wrappers reported by the framer.
- Independent Python inspection found **402 payload records attributed to PLP0** and **62,744 to PLP1** using receiver metadata. The file contains actual traffic from both simultaneously, not just two requested IDs in a UI.
- Six records remained unattributed and a few reserved packet types were preserved. This is a raw link-layer capture, not a claim that all broadcast payload/media was valid.
- Explicit PLP0-only and PLP1-only captures each delivered real ALP. Single-slot Sony output omitted repeated PLP context; the final tool provides the sole known explicitly configured slot as capture metadata.
- Final PLP1-only attribution check: **5,341 records**, **3,926,513 bytes**, only PLP1 observed, zero corrupt wrappers. This verifies the single-slot fallback without claiming it was an extra on-air field.

The provided HDHomeRun reference has identical global header bytes (`a1b23c4d0002000400000000000000000004000000000121`): big-endian classic PCAP 2.4, nanosecond fields, snap length 262144, ATSC ALP link type 289. Its 63,660 records span about 14.988 seconds. Captures differ naturally by station/time and host timestamp origin.

## Software checks

`./test.sh` passes synthetic checks for arbitrary USB splits (including one byte per append), synchronization, zero-payload F0/F1 metadata retention, sole-configured-PLP fallback, exact reference PCAP header, known timestamp encoding, fragmented file writes, correct PLP counters and refusal to overwrite an existing file.

`Tools/regenerate.py` reproduces the four generated hardware files byte for byte. An actual previously recorded Sony raw USB fixture converted successfully to 269 ALP-PCAP records. The fixture and local RF captures are not in the public source package.

No FreeStream hardware, iPad capture, protected-content decryption or ATSC 3.0 audio/video playback claim is made by this standalone release. GitHub Actions is provided for building/synthetic checks; it was not run on GitHub during local preparation.

## Internal app integration

The actual macOS Config button's model method completed a real 30-second Mate capture with PLPs 0 and 1: **135,017 records**, **99,431,518 bytes**, cancelled=false. Independent inspection attributed **420 payload records to PLP0** and **67,028 to PLP1**, with **zero untagged payloads**. The first-to-last packet timestamp span was 29.880 seconds; the 30-second recording window can contain intervals without packets.

The app's own ALP/IP/UDP parser recovered **67,208 UDP datagrams** and nine SLT services from the resulting file. Nine focused app receiver/parser/file-writer tests passed. The final macOS build succeeded. This verifies the same action invoked by the button; it is not a claim of a physical mouse-click UI automation test. The FreeStream capture button is wired to its existing ALP stream, but no FreeStream hardware was connected for this validation.

## 16 November 2026 — station-derived PLP presets

The same shared selection helper is included in the Mac app and standalone source package. Synthetic tests cover a two-PLP list, nonconsecutive IDs `3,17`, duplicate IDs, an unknown station list, four-slot limits and the existing automatic LLS/layer/ID ranking. No hardware testing of fictional/nonadvertised PLPs is implied. The CLI's chip-auto behavior is unchanged and is documented separately from the app's L1-driven automatic policy.

The final Mac focused suite passed **10 tests**, including the new shared PLP policy checks. The standalone capture tests and CLI build also passed after including the same shared helper.

## pcapmate portability package — 16 November 2026

One CLI/source tree now contains macOS/Linux toolchain discovery, a POSIX monotonic clock, Darwin/Glibc imports, Linux automatic kernel-driver handoff and a narrow udev rule. The command/executable is named **pcapmate**. Synthetic tests now explicitly include PLP16 and PLP63.

macOS: native build, shared tests and end-to-end CLI smoke checks passed during portability development. The smoke check exercises help, device-list JSON, Sony-wrapper replay, PLP16 attribution, independent PCAP inspection, JSON reports, no-overwrite and invalid-ID rejection. Prior actual-Mate RF results above remain macOS hardware evidence.

Linux: the native C backend compiled inside the official Swift 6.2 Ubuntu 24.04 ARM64 image. End-to-end CLI verification did **not** complete: internal-disk exhaustion corrupted the downloaded container runtime library (`libBlocksRuntime.so` contained zero bytes instead of an ELF header). This was a test-environment failure before the Swift program compiled, not a successful Linux test. The portable source and macOS/Linux x86-64/ARM64 CI are included for prompt upload; keep Linux verification marked pending until CI passes. Physical Linux USB/kernel-driver/RF reception remains untested even after software CI passes.

The temporary Linux VM was stopped after the test attempt. GitHub Actions has not been executed remotely because this project has not yet been uploaded.
