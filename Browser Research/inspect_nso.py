#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate an owned NSO's bounds and segment hashes; optional private unpacking."""
import argparse,hashlib,json,struct
from pathlib import Path

class NSO:
    def __init__(self,data):
        if len(data)<256 or data[:4]!=b'NSO0':raise ValueError('Not a complete NSO0 header')
        if len(data)>128*1024*1024:raise ValueError('Input exceeds analysis limit')
        self.source=data;self.flags=struct.unpack_from('<I',data,12)[0];self.segments=[]
        for index,name in enumerate(('text','rodata','data')):
            offset,address,size,extra=struct.unpack_from('<4I',data,16+16*index)
            compressed=bool(self.flags&(1<<index));stored=struct.unpack_from('<I',data,96+4*index)[0] if compressed else size
            if offset<256 or offset>len(data) or stored>len(data)-offset:raise ValueError('Segment outside file')
            if size>128*1024*1024 or address+size>256*1024*1024:raise ValueError('Segment exceeds analysis limit')
            raw=data[offset:offset+stored]
            if compressed:
                import lz4.block
                try:raw=lz4.block.decompress(raw,uncompressed_size=size)
                except Exception as error:raise ValueError('Invalid LZ4 segment') from error
            if len(raw)!=size:raise ValueError('Wrong unpacked segment length')
            actual=hashlib.sha256(raw).hexdigest();check=bool(self.flags&(1<<(index+3)))
            if check and actual!=data[160+index*32:192+index*32].hex():raise ValueError('Segment hash mismatch: '+name)
            self.segments.append({'name':name,'address':address,'size':size,'file_offset':offset,'stored_size':stored,'compressed':compressed,'hash_checked':check,'sha256':actual,'raw':raw})
        for space in ('address','file_offset'):
            intervals=sorted((s[space],s[space]+s['size' if space=='address' else 'stored_size']) for s in self.segments if s['size' if space=='address' else 'stored_size'])
            if any(a[1]>b[0] for a,b in zip(intervals,intervals[1:])):raise ValueError('Overlapping segments')

    def summary(self):
        return {'sha256':hashlib.sha256(self.source).hexdigest(),'build_id':self.source[64:96].hex(),'flags':self.flags,'segments':[{k:v for k,v in s.items() if k!='raw'} for s in self.segments]}

    def unpack(self,directory):
        directory=Path(directory);directory.mkdir(parents=True,exist_ok=False)
        for segment in self.segments:
            with (directory/(segment['name']+'.bin')).open('xb') as out:out.write(segment['raw'])
        (directory/'summary.json').write_text(json.dumps(self.summary(),indent=2)+'\n',encoding='utf-8')

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('nso',type=Path);parser.add_argument('--unpack',type=Path)
    args=parser.parse_args()
    if args.nso.stat().st_size>128*1024*1024:parser.error('Input exceeds analysis limit')
    nso=NSO(args.nso.read_bytes())
    if args.unpack:nso.unpack(args.unpack)
    print(json.dumps(nso.summary(),indent=2))

if __name__=='__main__':main()
