#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Independent classic ALP-PCAP structural/PLP inspection, no media dependencies."""
import collections, json, struct, sys

def inspect(path):
    with open(path, 'rb') as f:
        h = f.read(24)
        formats = {b'\xa1\xb2\x3c\x4d':('>',1e9), b'\x4d\x3c\xb2\xa1':('<',1e9), b'\xa1\xb2\xc3\xd4':('>',1e6), b'\xd4\xc3\xb2\xa1':('<',1e6)}
        if len(h) != 24 or h[:4] not in formats: raise ValueError('Invalid classic PCAP header')
        endian, scale = formats[h[:4]]
        _,major,minor,zone,sig,snap,link = struct.unpack(endian+'IHHIIII',h)
        if (major,minor,link) != (2,4,289): raise ValueError('Expected PCAP 2.4 / ALP link type 289')
        records = payloads = 0; first = last = None; context = None
        plps = collections.Counter(); types = collections.Counter(); untagged = 0
        while header := f.read(16):
            if len(header) != 16: raise ValueError('Truncated record header')
            sec,frac,n,original = struct.unpack(endian+'IIII',header)
            if n != original or n > snap or frac >= scale: raise ValueError('Invalid lengths/timestamp')
            b = f.read(n)
            if len(b) != n: raise ValueError('Truncated record')
            timestamp = sec+frac/scale; first = timestamp if first is None else first; last = timestamp
            records += 1
            if b[:5] == b'\x08\x00\x05\xf1\x00' and len(b) == 6: context = b[5] >> 2; continue
            if len(b) < 2: continue
            types[b[0] >> 5] += 1
            length = ((b[0]&7) << 8) | b[1]
            if length or (len(b)>2 and b[2]>>3):
                payloads += 1
                if context is not None: plps[context] += 1
                else: untagged += 1
                context = None
        return dict(records=records,payloadRecords=payloads,payloadsByPLP=dict(plps),untaggedPayloads=untagged,packetTypes=dict(types),spanSeconds=last-first if first is not None else 0,linkType=link,snapLength=snap,nanosecondFields=scale==1e9)

if __name__ == '__main__':
    print(json.dumps(inspect(sys.argv[1]),indent=2,sort_keys=True))
