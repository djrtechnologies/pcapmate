# Native and capture APIs

`Sources/Hardware/hdtvmate.h` is the C boundary:

```c
char error[512];
BDCMate *mate = bdc_mate_open(NULL, firmware_path, error, sizeof error);
int plps[] = {0, 1};
int result = bdc_mate_tune(mate, 485000000, 3, plps, 2); /* US RF16, 6 MHz ATSC 3 */
/* After result == 0, repeatedly drain USB: */
int count = bdc_mate_read(mate, buffer, sizeof buffer, 20);
/* count > 0: feed exactly those bytes to HDTVMateALPFramer.append. */
/* count == 0: timeout/no bytes; count < 0: stop and report transport error. */
bdc_mate_close(mate);
```

Check that open returned non-NULL before tuning. Serialize tuning, status and PLP control on one worker; continuously drain USB. The app uses a separate read worker so longer telemetry operations do not block draining. Stop/join a read worker before closing its handle. One process owns one physical receiver interface; do not open it independently in multiple tabs.

`bdc_mate_select_plps(mate, ids, count)` changes the simultaneous PLP slots of an already ATSC-3-tuned receiver. It accepts 1–4 unique IDs 0–63. `bdc_mate_tune` also accepts count 0 for initial automatic selection. A successful call is not proof of payload lock. `bdc_mate_status` exposes valid SNR/RSSI, demodulator and selected-slot state; observe actual ALP 0xF1 records for packet attribution.

Swift integration:

```swift
let framer = HDTVMateALPFramer()
let writer = try ALPPCAPFileWriter(url: outputURL)
framer.onRecord = { record in
    // Serialize and handle write errors in your owner; do not swallow failures.
    do { try writer.appendRecord(record) } catch { /* stop capture and report */ }
}
framer.fallbackPLPID = 0 // only if the hardware was explicitly configured with PLP0 alone
framer.append(usbBytes)
try writer.finish()
```

`ALPPCAPFileWriter.appendStream` alternatively accepts the exact fragmented PCAP stream produced by the app's Mate and FreeStream clients. It validates the shared header, writes complete records only, preserves their timestamps, counts payload records and records actually observed PLP IDs. The writer is synchronous; its owner serializes access off the main thread. It opens the destination exclusively and never overwrites existing files.

The macOS app's `USBTVPCAPCapture` wraps this writer with a 30-second monotonic deadline and progress callbacks, while its USB receiver hub arbitrates ownership across Config/Live/Player. Those app-specific UI/ownership types are outside this standalone library; they are unnecessary for CLI capture.

For iPad, the C USB transport needs a signed USBDriverKit user-client adapter. The macOS/Linux CLI uses libusb and does not imply that an arbitrary downloaded executable can run on iPad. The protocol/framer/PCAP writer are reusable; iPad packaging and driver support are a separate next step.

## Station-derived presets and automatic ranking

The shared `HDTVMatePLPSelection.presets(advertisedIDs:)` returns only cumulative combinations of unique valid advertised IDs, capped at four slots. For `[17,3]` the result is `[[3],[3,17]]`; for an unknown/empty list it is empty. This keeps the UI from inventing sequential IDs or displaying unavailable options. `automaticSelection(from:)` accepts decoded descriptors (`id`, `carriesLLS`, `layer`) and prioritizes LLS, then layer, then ID. See [PLP_SELECTION.md](PLP_SELECTION.md) for examples and the distinction between the app and CLI automatic modes.

Configured native slot registers are not an advertised station inventory. The app supplies validated L1 Detail IDs to this policy. The standalone CLI can accept explicit nonconsecutive IDs but does not itself enumerate L1 Detail.

On Linux, `bdc_mate_open` uses libusb automatic detach for a kernel-owned interface; only an interface successfully claimed by this handle is released on close. This lets libusb restore the detached kernel driver. Permission errors point to the supplied udev setup. The monotonic hardware timeout adapter is POSIX on both platforms.
