# Linux setup

This is the same `pcapmate` source and command syntax as macOS. Compile it on each OS; Linux uses an ELF executable and macOS uses Mach-O. Copying a Mac executable to Linux does not work.

## Build dependencies

Install a **Swift 6.2 or newer Linux toolchain** from https://www.swift.org/install/linux/ . The official instructions cover Swiftly and supported Linux distributions; Swift is a build requirement, not an Apple-only API in this program.

On Ubuntu/Debian, install the remaining build/test dependencies:

```sh
sudo apt-get update
sudo apt-get install -y clang libusb-1.0-0-dev pkg-config python3
```

On Fedora:

```sh
sudo dnf install clang libusb1-devel pkgconf-pkg-config python3
```

Then from this repository:

```sh
./build.sh
./test.sh
./Tools/smoke.sh
./build/pcapmate --list
```

The native backend uses POSIX clocks/pthreads and libusb. Swift imports Glibc on Linux and Darwin on macOS where needed. No Apple frameworks are linked on Linux. `CC`, `SWIFTC`, and `LIBUSB_PREFIX` can override compiler/library discovery. Source checkout paths containing spaces are supported. Runtime library paths are resolved against the locally installed libusb; install a compatible Swift runtime/toolchain when moving a dynamically linked executable to another host.

## USB permissions

A normal Linux user may need the supplied udev rule. It matches only the Mate's USB ID `23e2:2b02`; it does not grant world-writable access to every USB device.

```sh
getent group hdtvmate >/dev/null || sudo groupadd --system hdtvmate
sudo usermod -aG hdtvmate "$USER"
sudo install -m 0644 udev/70-hdtvmate.rules /etc/udev/rules.d/70-hdtvmate.rules
sudo udevadm control --reload-rules
```

Log out and back in for the group change, then unplug/reconnect the Mate. The `uaccess` tag additionally supports active desktop-seat access; group membership supports headless/server use. This rule is optional setup performed by the user, never installed automatically by the capture executable.

If an existing Linux DVB driver owns interface 0, libusb's automatic-detach mode temporarily releases that kernel driver's ownership during capture and reattaches it when the interface is released. Stop other DVB/receiver applications before capturing. This tool is a userspace driver and does not require installing the upstream kernel module. It will not allow two processes to independently tune the same physical receiver.

For USB access denied, check the rule, group membership, device reconnect and `/dev/bus/usb` permissions. For interface busy, stop other receiver software. Containers require explicit USB device access to capture real RF; the Docker/CI checks intentionally run without a dongle and exercise replay/tests instead.

## Capture

```sh
mkdir -p captures
./build/pcapmate --rf 16 --plps 0,1 --seconds 30 \
  --firmware /absolute/path/dvb-usb-it9303-01.fw \
  --output captures/rf16-plp0-1.pcap
```

Use the actual RF and advertised PLP IDs of your station. `--plps 16` and combinations such as `--plps 0,16` are valid; at most four simultaneous IDs are supported, with each ID 0–63. Firmware is supplied separately, as described in FIRMWARE.md.

## Repeatable container build/check

With Docker available, the following builds the Linux tool and runs synthetic and CLI replay tests, using an official Swift Ubuntu image:

```sh
docker build -f Tools/Dockerfile.linux-test -t pcapmate-check .
docker run --rm pcapmate-check
```

The same Dockerfile is used in CI on Linux x86-64 and ARM64 runners. See VALIDATION.md for what was actually executed locally versus CI that will run after upload. macOS RF tests do not prove Linux USB/kernel-driver behavior; physical Linux receiver testing remains a distinct check.
