// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 Dylan Ross.
import Foundation
#if canImport(Darwin)
import Darwin
#elseif canImport(Glibc)
import Glibc
#endif

@main struct Capture {
    static func main() {
        do { try run() } catch { fputs("Error: \(error.localizedDescription)\n",stderr); exit(1) }
    }
    static func run() throws {
        let args = Array(CommandLine.arguments.dropFirst())
        if args == ["--help"] || args.isEmpty {
            print("""
            pcapmate — A tool to get ATSC 3.0 packet captures from an HDTV Mate.
            GPL-2.0-only; no warranty. See LICENSE.
            --list
            --rf 16 --plps 0,1 --seconds 30 --firmware /path/bridge.fw --output /path/capture.pcap [--serial SERIAL]
            --plps auto selects the receiver's automatic PLP; explicit selection supports 1–4 unique IDs, 0–63.
            --replay /path/raw-alp-div-ts.bin --output /path/capture.pcap (offline converter)
            """); return
        }
        if args == ["--list"] {
            guard let p = bdc_mate_discover() else { throw fail("Discovery failed") }; defer { free(p) }; print(String(cString:p)); return
        }
        guard args.count % 2 == 0 else { throw fail("Options require values; use --help") }
        var options:[String:String] = [:]
        let allowed:Set<String> = ["--rf","--plps","--seconds","--firmware","--output","--serial","--replay"]
        for i in stride(from:0,to:args.count,by:2) {
            guard allowed.contains(args[i]),options[args[i]] == nil else { throw fail("Unknown or repeated option \(args[i])") }; options[args[i]] = args[i+1]
        }
        guard let output = options["--output"] else { throw fail("--output is required") }
        let ids = try plps(options["--plps"] ?? "auto")
        let url = URL(fileURLWithPath:output)
        let writer = try ALPPCAPFileWriter(url:url)
        let framer = HDTVMateALPFramer()
        framer.fallbackPLPID = options["--replay"] == nil && ids.count == 1 ? ids.first : nil
        var writeError:Error?
        framer.onRecord = { data in do { try writer.appendRecord(data) } catch { writeError = error } }
        if let replay = options["--replay"] {
            let file = try FileHandle(forReadingFrom:URL(fileURLWithPath:replay)); defer { try? file.close() }
            while let data = try file.read(upToCount:65_536),!data.isEmpty { framer.append(data);if let writeError { throw writeError } }
        } else {
            guard let rf = options["--rf"].flatMap(Int.init),(2...51).contains(rf),let firmware = options["--firmware"],
                  let seconds = Double(options["--seconds"] ?? "30"),seconds.isFinite,(1...3600).contains(seconds) else { throw fail("Valid --rf, --firmware and --seconds are required") }
            let hz = rf <= 4 ? 57_000_000+(rf-2)*6_000_000 : rf <= 6 ? 79_000_000+(rf-5)*6_000_000 : rf <= 13 ? 177_000_000+(rf-7)*6_000_000 : 473_000_000+(rf-14)*6_000_000
            var message = [CChar](repeating:0,count:512)
            let handle:OpaquePointer? = (options["--serial"] ?? "").withCString { serial in
                firmware.withCString { fw in bdc_mate_open(options["--serial"] == nil ? nil : serial,fw,&message,Int32(message.count)) }
            }
            guard let handle else { throw fail(String(cString:message)) }; defer { bdc_mate_close(handle) }
            let nativeIDs = ids.map(Int32.init)
            let tuned = bdc_mate_tune(handle,Int32(hz),3,nativeIDs,Int32(nativeIDs.count))
            guard tuned == 0 else { throw fail("Tune failed: \(tuned)") }
            fputs("RF\(rf), PLPs \(ids.isEmpty ? "automatic" : ids.map(String.init).joined(separator:",")); waiting up to 20 s for ALP…\n",stderr)
            let wait = ProcessInfo.processInfo.systemUptime+20
            var start:TimeInterval?,lastPoll = 0.0,buffer = [UInt8](repeating:0,count:188*348)
            while true {
                let now = ProcessInfo.processInfo.systemUptime
                if let start { if now-start >= seconds { break } }
                else if now > wait { throw fail("No complete ALP record within 20 seconds; capture contains header only") }
                let n = bdc_mate_read(handle,&buffer,Int32(buffer.count),20)
                guard n >= 0 else { throw fail("USB read failed: \(n)") }
                if n > 0 { framer.append(Data(buffer.prefix(Int(n)))) }
                if let writeError { throw writeError }
                if writer.records > 0,start == nil { start = ProcessInfo.processInfo.systemUptime }
                if now-lastPoll >= 1 {
                    lastPoll = now
                    var status = BDCMateStatus();let result = bdc_mate_status(handle,&status)
                    fputs(String(format:"%.1fs · %d records · %llu bytes · lock %d · SNR %.2f dB (status %d)\n",start.map { now-$0 } ?? 0,writer.records,writer.bytes,status.locked,status.snr_db,result),stderr)
                }
            }
        }
        try writer.finish()
        let report:[String:Any] = ["file":url.lastPathComponent,"records":writer.records,"payloadRecords":writer.payloadRecords,"bytes":writer.bytes,"requestedPLPs":ids,"observedPLPs":writer.observedPLPs.sorted(),"corruptWrappers":framer.corruptWrappers,"linkType":289,"plpAttribution":"device F1 metadata; sole explicitly configured PLP fallback when omitted","timestampFields":"nanoseconds","timestampSource":"host USB arrival; not RF time"]
        try JSONSerialization.data(withJSONObject:report,options:[.prettyPrinted,.sortedKeys]).write(to:url.appendingPathExtension("json"),options:.atomic)
        print("Saved \(url.path) · \(writer.records) records · observed PLPs \(writer.observedPLPs.sorted())")
        guard writer.payloadRecords > 0 else { throw fail("No payload records recovered") }
    }
    static func plps(_ text:String) throws -> [Int] {
        if text == "auto" { return [] }
        let tokens = text.split(separator:",",omittingEmptySubsequences:false),ids = tokens.compactMap { Int($0) }
        guard (1...4).contains(ids.count),ids.count == tokens.count,Set(ids).count == ids.count,ids.allSatisfy({ (0...63).contains($0) }) else { throw fail("PLPs must be 1–4 unique IDs from 0 to 63, or auto") }
        return ids.sorted()
    }
    static func fail(_ text:String)->NSError { NSError(domain:"HDTVMate",code:1,userInfo:[NSLocalizedDescriptionKey:text]) }
}
