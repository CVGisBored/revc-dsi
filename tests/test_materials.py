import sys
from pathlib import Path
import struct
import unittest
import numpy as np
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from materials import handheld_texture
from stage_world import clip

class MaterialsTests(unittest.TestCase):
    def test_clipping_interpolates_uv(self):
        tri=[(-1,0,0,0,0),(1,0,0,1,0),(1,0,2,1,1)]
        clipped=clip(tri,0,0,True)
        self.assertEqual(len(clipped),4)
        for x,y,z,u,v in clipped:
            self.assertAlmostEqual(u,(x+1)/2);self.assertAlmostEqual(v,z/2)
    def test_original_color_palette(self):
        raw,error=handheld_texture(Image.new('RGBA',(16,16),(255,0,0,255)))
        self.assertEqual(len(raw),4612);self.assertEqual(struct.unpack_from('<I',raw)[0],0)
        colors=struct.unpack_from('<256H',raw,4);self.assertEqual(colors[raw[516]],31)
        self.assertEqual(error,0)
    def test_cutout_alpha_preserves_opaque_black(self):
        image=Image.new('RGBA',(64,64),(0,0,0,255));image.putpixel((0,0),(255,0,255,0))
        raw,_=handheld_texture(image)
        self.assertEqual(struct.unpack_from('<I',raw)[0],1)
        self.assertEqual(raw[516],0);self.assertNotEqual(raw[517],0)

if __name__=='__main__':unittest.main()
