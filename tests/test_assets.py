import sys
import struct
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from assets import FormatError, Instance, chunks, compose, geometry, place, transform, IDENTITY, visible_at


def chunk(kind, payload, version=0x1003ffff):
    return struct.pack('<III', kind, len(payload), version) + payload


def triangle_geometry(index=2):
    # One untextured morph target containing a single known triangle.
    body = struct.pack('<IIII', 0, 1, 3, 1)
    body += struct.pack('<HHHH', 1, 0, 0, index)
    body += struct.pack('<4fII', 0, 0, 0, 1, 1, 0)
    body += struct.pack('<9f', 0, 0, 0, 1, 0, 0, 0, 1, 0)
    return chunk(1, body)


class AssetsTests(unittest.TestCase):
    def test_geometry_triangle_order_and_vertices(self):
        vertices, triangles = geometry(triangle_geometry())
        self.assertEqual(triangles, [(0, 1, 2)])
        self.assertEqual(vertices, [(0, 0, 0), (1, 0, 0), (0, 1, 0)])

    def test_bad_index(self):
        with self.assertRaises(FormatError):
            geometry(triangle_geometry(3))

    def test_truncation(self):
        for data in (b'123', triangle_geometry()[:-1], chunk(1, b'123')):
            with self.assertRaises(FormatError):
                geometry(data)

    def test_archive_padding(self):
        data = chunk(16, b'123') + b'\0' * 2048
        self.assertEqual(list(chunks(data, padded=True))[0][2], b'123')
        with self.assertRaises(FormatError):
            list(chunks(chunk(1, b'123') + b'\0'))

    def test_transform_composition(self):
        parent = IDENTITY[:9] + (2., 3., 4.)
        child = (0., 1., 0., -1., 0., 0., 0., 0., 1., 5., 6., 7.)
        point = (1., 2., 3.)
        self.assertEqual(transform(compose(parent, child), point),
                         transform(parent, transform(child, point)))

    def test_placement_scale_translation(self):
        instance = Instance(1, 'test', (10, 20, 30), (2, 3, 4), (0, 0, 0, 1), '')
        self.assertEqual(place(instance, (1, 2, 3)), (12, 26, 42))
        instance.rotation = (0, 0, 0, 0)
        with self.assertRaises(FormatError):
            place(instance, (1, 2, 3))

    def test_day_night_ranges(self):
        self.assertTrue(visible_at((5, 20), 12))
        self.assertFalse(visible_at((5, 20), 20))
        self.assertTrue(visible_at((20, 5), 0))
        self.assertTrue(visible_at((20, 5), 23))
        self.assertFalse(visible_at((20, 5), 12))


if __name__ == '__main__':
    unittest.main()
