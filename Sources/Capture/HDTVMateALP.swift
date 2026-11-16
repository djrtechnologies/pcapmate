// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 Dylan Ross. Modified 2026-11-15: retain metadata; HDHomeRun-style PCAP.
import Foundation

/// Sony ALP-div-TS uses a three-byte header, not the MPEG-TS four-byte header.
/// USB transfers can end anywhere, including inside a 188-byte wrapper.
nonisolated final class HDTVMateALPFramer {
    private var pending = Data()
    private var alp = Data()
    private var active = false
    private var hasPLPContext = false
    // A sole explicitly configured hardware slot is unambiguous even when Sony omits F1.
    var fallbackPLPID: Int?
    private var synchronized = false
    private(set) var corruptWrappers = 0
    var onRecord: ((Data) -> Void)?

    func append(_ data: Data) {
        pending.append(data)
        var cursor = 0
        while pending.count - cursor >= 188 {
            if !synchronized {
                guard pending.count - cursor >= 376 else { break }
                guard pending[cursor] == 0x47, pending[cursor + 188] == 0x47 else {
                    cursor += 1; active = false; alp.removeAll(keepingCapacity: true); continue
                }
                synchronized = true
            }
            guard pending[cursor] == 0x47 else {
                synchronized = false; active = false; alp.removeAll(keepingCapacity: true); continue
            }
            let packet = Array(pending[cursor..<(cursor + 188)])
            cursor += 188
            if packet[1] & 0x80 != 0 {
                corruptWrappers += 1; active = false; alp.removeAll(keepingCapacity: true); continue
            }
            let pid = (Int(packet[1] & 31) << 8) | Int(packet[2])
            if pid == 8191 { continue }
            if packet[1] & 0x40 != 0 {
                let pointer = Int(packet[3])
                guard pointer <= 184 else { active = false; alp.removeAll(); continue }
                if active { alp.append(contentsOf: packet[4..<(4 + pointer)]); drain() }
                alp = Data(packet[(4 + pointer)...]); active = true; drain()
            } else if active {
                alp.append(contentsOf: packet[3...]); drain()
            }
            if alp.count > 131_072 { active = false; alp.removeAll() }
        }
        if cursor > 0 { pending = Data(pending.dropFirst(cursor)) }
    }

    private func drain() {
        while alp.count >= 2 {
            let b = [UInt8](alp.prefix(64))
            let type = b[0] >> 5, pc = b[0] & 0x10 != 0, mode = b[0] & 0x08 != 0
            var length = (Int(b[0] & 7) << 8) | Int(b[1]), header = 2
            if pc || mode {
                guard b.count >= 3 else { return }
                let extra = b[2]; header = 3
                if !pc { length |= Int(extra >> 3) << 11 }
                if pc && mode {
                    length |= Int(extra >> 4) << 11
                    let fields = Int((extra >> 1) & 7) + 1
                    header += (fields * 12 + 7) / 8
                    if extra & 1 != 0 { header += 1 }
                } else {
                    if extra & 2 != 0 { header += 1 }
                    if extra & 1 != 0 {
                        guard alp.count >= header + 2 else { return }
                        header += 2 + Int(alp[header + 1]) + 1
                    }
                }
            }
            if type == 4 { header += 5 }
            if type == 6 { header += 2 }
            let total = header + length
            guard total <= 131_072 else { active = false; alp.removeAll(); return }
            guard alp.count >= total else { return }
            let record = Data(alp.prefix(total))
            alp = Data(alp.dropFirst(total))
            if record.count == 6, Array(record.prefix(5)) == [8,0,5,0xf1,0] { hasPLPContext = true }
            if length > 0 {
                if !hasPLPContext,let id = fallbackPLPID,(0...63).contains(id) {
                    onRecord?(Data([8,0,5,0xf1,0,UInt8(id << 2)]))
                }
                hasPLPContext = false
            }
            // Preserve zero-payload PLP and timestamp extensions, not just IP payloads.
            onRecord?(record)
        }
    }

    // Match the supplied HDHomeRun capture: network byte order, nanosecond fields, DLT 289.
    static var pcapHeader: Data {
        var result = Data([0xa1, 0xb2, 0x3c, 0x4d, 0, 2, 0, 4])
        for v: UInt32 in [0, 0, 262_144, 289] { var be = v.bigEndian; withUnsafeBytes(of: &be) { result.append(contentsOf: $0) } }
        return result
    }
    static func pcapRecord(_ record: Data, at time: TimeInterval = Date().timeIntervalSince1970) -> Data {
        var result = Data()
        let seconds = floor(time)
        for v in [UInt32(seconds), UInt32(min(999_999_999, max(0, (time-seconds)*1_000_000_000))), UInt32(record.count), UInt32(record.count)] {
            var be = v.bigEndian; withUnsafeBytes(of: &be) { result.append(contentsOf: $0) }
        }
        result.append(record); return result
    }
}
