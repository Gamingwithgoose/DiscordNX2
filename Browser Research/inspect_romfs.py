#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded, read-only RomFS inspection; optional extraction into a NEW directory."""
import argparse,hashlib,json,struct
from pathlib import Path

class RomFS:
    def __init__(self, data):
        self.data=data
        if len(data)<80:raise ValueError('Truncated RomFS header')
        h=struct.unpack_from('<10Q',data)
        if h[0]!=80:raise ValueError('Unsupported RomFS header')
        for offset,size in zip(h[1:9:2],h[2:9:2]):
            if offset<80 or offset>len(data) or size>len(data)-offset:raise ValueError('Table outside image')
        self.dir_off,self.dir_size=h[3:5];self.file_off,self.file_size=h[7:9];self.data_off=h[9]
        self.data_end=min(h[1],h[3],h[5],h[7])
        if not 80<=self.data_off<=self.data_end:raise ValueError('Invalid file data bounds')
        self.dirs=set();self.files=set();self.paths=set();self.entries=[]

    def entry(self,offset,directory):
        base,size,fmt=(self.dir_off,self.dir_size,'<6I') if directory else (self.file_off,self.file_size,'<IIQQII')
        length=struct.calcsize(fmt)
        if offset>size or length>size-offset:raise ValueError('Truncated metadata entry')
        fields=struct.unpack_from(fmt,self.data,base+offset);n=fields[-1]
        if n>size-offset-length:raise ValueError('Truncated entry name')
        name=self.data[base+offset+length:base+offset+length+n].decode('utf-8')
        if name and (name in ('.','..') or any(c in name for c in '/\\:\x00') or any(ord(c)<32 for c in name)):raise ValueError('Unsafe name')
        return fields,name

    def walk(self,offset=0,parent=0,path='',depth=0):
        if depth>32 or offset in self.dirs or len(self.dirs)+len(self.files)>=10000:raise ValueError('Directory cycle or metadata limit')
        self.dirs.add(offset);d,name=self.entry(offset,True)
        if offset and (d[0]!=parent or not name):raise ValueError('Invalid directory parent/name')
        if not offset and name:raise ValueError('Root must be unnamed')
        current=(path+'/'+name).lstrip('/')
        child=d[3]
        while child!=0xffffffff:
            if child in self.files or len(self.dirs)+len(self.files)>=10000:raise ValueError('File cycle or metadata limit')
            self.files.add(child);f,name=self.entry(child,False)
            if f[0]!=offset or not name:raise ValueError('Invalid file parent/name')
            full=(current+'/'+name).lstrip('/')
            if full in self.paths:raise ValueError('Duplicate output path')
            self.paths.add(full);at=self.data_off+f[2];size=f[3]
            if at>self.data_end or size>self.data_end-at:raise ValueError('File outside data area')
            blob=self.data[at:at+size]
            self.entries.append({'path':full,'offset':at,'size':size,'sha256':hashlib.sha256(blob).hexdigest()})
            child=f[1]
        child=d[2]
        while child!=0xffffffff:
            e,_=self.entry(child,True);self.walk(child,offset,current,depth+1);child=e[1]
        return self.entries

    def extract(self, destination):
        destination=Path(destination)
        destination.mkdir(parents=True,exist_ok=False);root=destination.resolve()
        for entry in self.entries:
            target=destination/entry['path']
            if not target.resolve().is_relative_to(root):raise ValueError('Path escapes output directory')
            target.parent.mkdir(parents=True,exist_ok=True)
            with target.open('xb') as out:out.write(self.data[entry['offset']:entry['offset']+entry['size']])

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path);parser.add_argument('--extract',type=Path);parser.add_argument('--manifest',type=Path)
    args=parser.parse_args()
    if args.manifest and args.manifest.resolve()==args.image.resolve():parser.error('Manifest cannot replace the input image')
    if args.image.stat().st_size>512*1024*1024:parser.error('Image exceeds 512 MiB limit')
    fs=RomFS(args.image.read_bytes());entries=fs.walk()
    if args.extract:fs.extract(args.extract)
    manifest={'image_sha256':hashlib.sha256(fs.data).hexdigest(),'files':entries,'count':len(entries)}
    if args.manifest:args.manifest.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print('Files:',len(entries),'directories:',len(fs.dirs))
    for e in entries:
        if e['path'].endswith(('.nro','.nrr','.dat','.json','.txt')) or 'whitelist' in e['path'].lower():print(e['path'],e['size'])

if __name__=='__main__':main()
