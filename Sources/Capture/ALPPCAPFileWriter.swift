// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 Dylan Ross.
import Foundation
#if canImport(Darwin)
import Darwin
#elseif canImport(Glibc)
import Glibc
#endif

/// Shared by the app and standalone capture tool. Caller serializes access.
nonisolated final class ALPPCAPFileWriter {
    let url: URL
    private let file: FileHandle
    private var pending = Data()
    private var needsHeader = true
    private var context: Int?
    private(set) var records = 0
    private(set) var bytes: UInt64 = 24
    private(set) var observedPLPs: Set<Int> = []
    private(set) var payloadRecords = 0
    private(set) var closed = false
    init(url: URL) throws {
        self.url = url
        // Never replace an existing capture, including from another app/process.
        let fd = open(url.path, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)
        guard fd >= 0 else { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        file = FileHandle(fileDescriptor: fd, closeOnDealloc: true)
        do { try file.write(contentsOf: HDTVMateALPFramer.pcapHeader) }
        catch { try? file.close(); throw error }
    }
    func appendRecord(_ record: Data, at time: TimeInterval = Date().timeIntervalSince1970) throws {
        try writeFramed(HDTVMateALPFramer.pcapRecord(record, at: time), payload: record)
    }
    /// Accepts the exact complete/fragmented stream produced by both USB clients.
    func appendStream(_ data: Data) throws {
        guard !closed else { return }
        pending.append(data)
        var offset = 0
        if needsHeader {
            guard pending.count >= 24 else { return }
            guard pending.prefix(24) == HDTVMateALPFramer.pcapHeader else { throw Self.invalid("Unexpected ALP-PCAP stream header") }
            needsHeader = false; offset = 24
        }
        func u32(_ i: Int) -> Int { pending[i..<(i+4)].reduce(0) { ($0 << 8) | Int($1) } }
        while pending.count-offset >= 16 {
            let n = u32(offset+8), original = u32(offset+12)
            guard n <= 262_144, n == original, u32(offset+4) < 1_000_000_000 else { throw Self.invalid("Invalid ALP-PCAP record") }
            guard pending.count-offset >= 16+n else { break }
            try writeFramed(Data(pending[offset..<(offset+16+n)]), payload: Data(pending[(offset+16)..<(offset+16+n)]))
            offset += 16+n
        }
        if offset > 0 { pending = Data(pending.dropFirst(offset)) }
    }
    private func writeFramed(_ framed: Data, payload: Data) throws {
        guard !closed else { throw Self.invalid("Capture already closed") }
        try file.write(contentsOf: framed)
        records += 1; bytes += UInt64(framed.count)
        let b = [UInt8](payload)
        if b.count == 6, Array(b.prefix(5)) == [8,0,5,0xf1,0] { context = Int(b[5] >> 2) }
        else if b.count >= 2, (Int(b[0]&7)<<8 | Int(b[1])) > 0 || (b.count >= 3 && b[2] >> 3 > 0) {
            payloadRecords += 1
            if let context { observedPLPs.insert(context) }
            context = nil
        }
    }
    func finish() throws {
        guard !closed else { return }
        closed = true
        defer { try? file.close() }
        try file.synchronize()
    }
    private static func invalid(_ text: String) -> NSError { NSError(domain:"ALPPCAP",code:1,userInfo:[NSLocalizedDescriptionKey:text]) }
}
