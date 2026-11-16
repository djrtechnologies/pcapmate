#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# End-to-end CLI/file checks; no receiver or firmware required.
set -eu
cd "$(dirname "$0")/.."
TASK_TEST_DIR="$(mktemp -d)"
trap 'rm -rf "$TASK_TEST_DIR"' EXIT HUP INT TERM
build/pcapmate --help > "$TASK_TEST_DIR/help.txt"
build/pcapmate --list > "$TASK_TEST_DIR/devices.json"
python3 - "$TASK_TEST_DIR" <<'PY'
import json,sys
from pathlib import Path
folder=Path(sys.argv[1])
assert isinstance(json.loads((folder/'devices.json').read_text()),list)
# One synthetic ALP payload on PLP16, inside Sony's 3-byte-header wrapper.
context=bytes([8,0,5,0xf1,0,16<<2]); payload=bytes([0,20])+bytes([0x45])*20
wrapper=bytes([0x47,0x40,0,0])+context+payload
wrapper+=bytes([0xff])*(188-len(wrapper))
null=bytes([0x47,0x1f,0xff])+bytes([0xff])*185
(folder/'input.bin').write_bytes(wrapper+null)
PY
build/pcapmate --replay "$TASK_TEST_DIR/input.bin" --output "$TASK_TEST_DIR/output.pcap"
python3 Tools/inspect-pcap.py "$TASK_TEST_DIR/output.pcap" > "$TASK_TEST_DIR/inspection.json"
python3 - "$TASK_TEST_DIR" <<'PY'
import json,sys
from pathlib import Path
folder=Path(sys.argv[1]); result=json.loads((folder/'inspection.json').read_text())
assert result['linkType']==289 and result['nanosecondFields']
assert result['payloadsByPLP']=={'16':1} and result['untaggedPayloads']==0
assert json.loads((folder/'output.pcap.json').read_text())['observedPLPs']==[16]
PY
if build/pcapmate --replay "$TASK_TEST_DIR/input.bin" --output "$TASK_TEST_DIR/output.pcap" >/dev/null 2>&1; then
    echo 'CLI overwrote an existing capture' >&2; exit 1
fi
if build/pcapmate --plps 64 --output "$TASK_TEST_DIR/invalid.pcap" >/dev/null 2>&1; then
    echo 'CLI accepted an invalid PLP ID' >&2; exit 1
fi
printf 'Passed CLI smoke checks: help, USB enumeration, raw replay, PLP16, PCAP/JSON, no overwrite, invalid ID rejection\n'
