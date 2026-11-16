// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 Dylan Ross.
import Foundation

@main struct Tests {
    static func main() throws {
        func check(_ condition:Bool,_ message:String) { if !condition { fatalError(message) } }
        check(HDTVMatePLPSelection.presets(advertisedIDs:[17,3,3]) == [[3],[3,17]],"Presets invented IDs or unavailable combinations")
        check(HDTVMatePLPSelection.presets(advertisedIDs:[]) == [],"Unknown signaling invented PLPs")
        check(HDTVMatePLPSelection.presets(advertisedIDs:[2,5,17,33,63]).last == [2,5,17,33],"Preset exceeds four simultaneous slots")
        check(HDTVMatePLPSelection.automaticSelection(from:[
            .init(id:17,carriesLLS:true,layer:1),.init(id:3,carriesLLS:false,layer:0),
            .init(id:8,carriesLLS:false,layer:1),.init(id:5,carriesLLS:false,layer:0),
            .init(id:63,carriesLLS:false,layer:1)]) == [17,3,5,8],"Automatic ranking changed")
        let context = Data([8,0,5,0xf1,0,4])
        let stamp = Data([8,0,5,0xf0,7,0,0,0,0,0,0,0,0])
        let payload = Data([0,40])+Data(repeating:0x45,count:40)
        // Sony uses a 3-byte wrapper header, then a PUSI pointer. Null wrappers are ignored.
        let alp = context+stamp+payload
        var wrapper = Data([0x47,0x40,0,0]);wrapper.append(alp)
        wrapper.append(Data(repeating:0xff,count:188-wrapper.count))
        let null = Data([0x47,0x1f,0xff])+Data(repeating:0xff,count:185)
        let input = Data([1,2,3])+wrapper+null
        func decoded(_ step:Int)->[Data] {
            let framer = HDTVMateALPFramer();var records:[Data] = []
            framer.onRecord = { records.append($0) }
            for i in stride(from:0,to:input.count,by:step) { framer.append(input.subdata(in:i..<min(i+step,input.count))) }
            return records
        }
        for n in [1,2,3,7,73,187,188,189,376,4096] {
            let records = decoded(n)
            check(Array(records.prefix(3)) == [context,stamp,payload],"Metadata/data changed at USB split \(n)")
        }
        for id in [0,7,16,63] {
            let single = HDTVMateALPFramer();single.fallbackPLPID = id
            var singleRecords:[Data] = [];single.onRecord = { singleRecords.append($0) }
            var singleWrapper = Data([0x47,0x40,0,0])+payload
            singleWrapper.append(Data(repeating:0xff,count:188-singleWrapper.count))
            single.append(singleWrapper+null)
            check(Array(singleRecords.prefix(2)) == [Data([8,0,5,0xf1,0,UInt8(id<<2)]),payload],"Sole configured PLP context missing: \(id)")
        }
        check(HDTVMateALPFramer.pcapHeader.map { String(format:"%02x",$0) }.joined() == "a1b23c4d0002000400000000000000000004000000000121","HDHomeRun header mismatch")
        let framed = HDTVMateALPFramer.pcapRecord(payload,at:1234.125)
        check(Array(framed.prefix(8)) == [0,0,4,210,7,115,89,64],"Seconds/nanoseconds mismatch")
        let folder = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString,isDirectory:true)
        try FileManager.default.createDirectory(at:folder,withIntermediateDirectories:true)
        defer { try? FileManager.default.removeItem(at:folder) }
        let url = folder.appendingPathComponent("test.pcap"),writer = try ALPPCAPFileWriter(url:url)
        let stream = HDTVMateALPFramer.pcapHeader+HDTVMateALPFramer.pcapRecord(context,at:1234)+framed
        for byte in stream { try writer.appendStream(Data([byte])) }
        try writer.finish();try writer.finish()
        check(try Data(contentsOf:url) == stream,"Fragmented writer changed stream")
        check(writer.records == 2 && writer.payloadRecords == 1 && writer.observedPLPs == [1],"Record/PLP counters incorrect")
        do { _ = try ALPPCAPFileWriter(url:url);fatalError("Writer overwrote existing capture") } catch {}
        print("Passed: advertised-ID presets, automatic ranking, PLP0/7/16/63, USB splits, metadata retention, reference PCAP header, nanosecond timestamps, split file writes, PLP context, no overwrite")
    }
}
