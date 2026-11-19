# SPDX-License-Identifier: MIT
import hashlib,struct,tempfile,unittest
from pathlib import Path
from inspect_nso import NSO

def sample():
    b=bytearray(268);b[:4]=b'NSO0';struct.pack_into('<I',b,12,0x38)
    for i,raw in enumerate((b'TEXT',b'RODT',b'DATA')):
        struct.pack_into('<4I',b,16+i*16,256+i*4,i*4096,4,0)
        b[256+i*4:260+i*4]=raw;b[160+i*32:192+i*32]=hashlib.sha256(raw).digest()
    return b

class NsoTests(unittest.TestCase):
    def test_valid(self):
        nso=NSO(sample());self.assertEqual(len(nso.segments),3);self.assertTrue(all(x['hash_checked'] for x in nso.segments))
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp)/'private';nso.unpack(out);self.assertEqual((out/'text.bin').read_bytes(),b'TEXT')
            with self.assertRaises(FileExistsError):nso.unpack(out)

    def test_corrupt(self):
        b=sample();b[-1]^=1
        with self.assertRaises(ValueError):NSO(b)

    def test_truncated(self):
        for n in (0,255,267):
            with self.assertRaises(ValueError):NSO(sample()[:n])

    def test_memory_limit(self):
        b=sample();struct.pack_into('<I',b,20,0xffffffff)
        with self.assertRaises(ValueError):NSO(b)

    def test_overlapping(self):
        b=sample();struct.pack_into('<I',b,36,0)
        with self.assertRaises(ValueError):NSO(b)

    def test_compressed(self):
        try:import lz4.block
        except ImportError:self.skipTest('Optional LZ4 dependency not installed')
        b=sample();raw=b'TEXT';encoded=lz4.block.compress(raw,store_size=False);struct.pack_into('<I',b,12,0x39)
        b.extend(bytes(len(encoded)));struct.pack_into('<I',b,16,len(b)-len(encoded));struct.pack_into('<I',b,96,len(encoded));b[-len(encoded):]=encoded
        self.assertEqual(NSO(b).segments[0]['raw'],raw)

if __name__=='__main__':unittest.main()
