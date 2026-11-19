# SPDX-License-Identifier: MIT
import hashlib,struct,tempfile,unittest
from pathlib import Path
from inspect_romfs import RomFS

def image():
    data=bytearray(588)
    struct.pack_into('<10Q',data,0,80,520,4,524,24,548,4,552,36,512)
    struct.pack_into('<6I',data,524,0,0xffffffff,0xffffffff,0,0xffffffff,0)
    struct.pack_into('<IIQQII',data,552,0,0xffffffff,0,3,0xffffffff,4)
    data[584:588]=b'test';data[512:515]=b'abc'
    return data

class RomFSTests(unittest.TestCase):
    def test_read_and_extract(self):
        data=image();before=hashlib.sha256(data).digest();fs=RomFS(data);files=fs.walk()
        self.assertEqual(len(files),1);self.assertEqual(files[0]['path'],'test')
        self.assertEqual(files[0]['sha256'],hashlib.sha256(b'abc').hexdigest())
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp)/'fresh';fs.extract(out);self.assertEqual((out/'test').read_bytes(),b'abc')
            with self.assertRaises(FileExistsError):fs.extract(out)
        self.assertEqual(hashlib.sha256(data).digest(),before)

    def test_truncated(self):
        for size in (0,79,100,550):
            with self.assertRaises(ValueError):RomFS(image()[:size]).walk()

    def test_directory_cycle(self):
        data=image();struct.pack_into('<I',data,524+8,0)
        with self.assertRaises(ValueError):RomFS(data).walk()

    def test_file_cycle(self):
        data=image();struct.pack_into('<I',data,552+4,0)
        with self.assertRaises(ValueError):RomFS(data).walk()

    def test_bad_names(self):
        for name in (b'../x',b'a\\bc',b'a:bc',b'a\0bc',b'\xffabc'):
            data=image();data[584:588]=name
            with self.assertRaises(ValueError):RomFS(data).walk()

    def test_data_bounds(self):
        data=image();struct.pack_into('<Q',data,552+8,99)
        with self.assertRaises(ValueError):RomFS(data).walk()

    def test_wrong_parent(self):
        data=image();struct.pack_into('<I',data,552,20)
        with self.assertRaises(ValueError):RomFS(data).walk()

if __name__=='__main__':unittest.main()
