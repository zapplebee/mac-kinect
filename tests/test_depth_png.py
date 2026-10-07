"""Check the depth PNG/mask wire output, independently of a connected camera."""
import struct
import unittest
import zlib
from demo.server import png, WIDTH, HEIGHT


def read_scanlines(data):
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    offset = 8
    compressed = bytearray()
    while offset < len(data):
        size = struct.unpack_from('>I', data, offset)[0]
        kind = data[offset+4:offset+8]
        payload = data[offset+8:offset+8+size]
        crc = struct.unpack_from('>I', data, offset+8+size)[0]
        assert crc == zlib.crc32(kind+payload)
        if kind == b'IDAT':
            compressed.extend(payload)
        offset += 12+size
    return zlib.decompress(compressed)


class DepthPngTest(unittest.TestCase):
    def test_distance_endpoints_and_invalid(self):
        depths = struct.pack('<6H', 0, 100, 500, 2250, 4000, 5000)
        depths += b'\0\0' * (WIDTH*HEIGHT-6)
        scan = read_scanlines(png(depths, 500, 4000, 0))
        self.assertEqual(scan[:7], bytes([0, 0, 255, 255, 128, 0, 0]))
        self.assertEqual(len(scan), (WIDTH+1)*HEIGHT)

    def test_mask_never_keys_in_invalid_depth(self):
        depths = struct.pack('<4H', 0, 1499, 1500, 1501)
        depths += b'\0\0' * (WIDTH*HEIGHT-4)
        scan = read_scanlines(png(depths, 500, 4000, 1500))
        self.assertEqual(scan[:5], bytes([0, 0, 255, 255, 0]))


if __name__ == '__main__':
    unittest.main()
