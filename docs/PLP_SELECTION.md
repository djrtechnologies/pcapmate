# PLP IDs, presets and automatic selection

PLP IDs are six-bit values **0–63**. They do not have to start at zero, be consecutive, or match the receiver's hardware slot index. A station might advertise `3` and `17`; those are the values to pass to the native API or CLI.

## Internal macOS app

The Config presets are built from unique IDs in the Mate's decoded, CRC-validated **L1 Detail** advertisement. They do not use the configured Sony slot registers as an inventory of the station's PLPs.

Examples:

| Advertised IDs | Explicit preset buttons |
| --- | --- |
| `0` | `0` |
| `0,1` | `0`, `0+1` |
| `3,17` | `3`, `3+17` |
| `2,5,17` | `2`, `2+5`, `2+5+17` |

Automatic and Custom remain available separately. No unsupported longer combination appears when a station advertises only one or two PLPs. Before valid L1 Detail arrives, the UI shows that it is waiting for the station list; it does not invent PLP0/PLP1. Presets apply immediately, and selecting Automatic clears earlier manual slot selections. Custom supports any explicit combination of up to four unique IDs in range.

Preset combinations use ascending advertised IDs and are capped at four simultaneous PLPs. For a station advertising more than four, Custom can select a different four-ID subset; the tuner cannot decode all 64 at once.

Automatic first permits the chip's automatic acquisition. Once valid L1 Detail is available, the app selects up to four unique advertised PLPs with this priority:

1. PLPs advertising LLS signaling.
2. Lower layer number (core before enhanced when other priorities tie).
3. Lower PLP ID.

It is not strongest-SNR ranking. Successful selection still depends on RF/FEC lock; a requested PLP is not evidence that its payload was received.

`Sources/Capture/HDTVMatePLPSelection.swift` contains the exact shared preset/ranking policy used by the app. It accepts advertised descriptors/IDs from a caller's L1 decoder. The whole internal SwiftUI application and its L1 parser are not bundled in this standalone capture release.

## Standalone CLI

The CLI already accepts arbitrary explicit IDs:

```sh
./build/pcapmate --rf 16 --plps 3,17 --seconds 30 \
  --firmware /path/to/bridge.fw --output captures/example.pcap
```

The frequency and IDs above are illustrative; use the actual station's RF and PLPs. Explicit IDs configure simultaneous native hardware slots.

**CLI `--plps auto` differs from the full app:** it leaves selection to the chip's initial automatic mode. The CLI does not independently decode L1 Detail to enumerate the station or invoke the app's higher-level ranking policy. Neither auto mode promises every PLP. Use explicit IDs for reproducible comparisons/captures. Packet-level F1 metadata and the sidecar's observed IDs show which PLPs actually delivered data.

Tests cover two advertised PLPs, nonconsecutive `3,17`, duplicate advertisements, unknown signaling, four-slot limits and LLS/layer/ID priority. Those nonconsecutive-ID checks are synthetic; the local hardware station used PLPs0/1, and no different real-world PLP inventory is claimed.
