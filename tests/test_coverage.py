import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from stage_coverage import dissolve,cross,sub

def face(a,b,c,texture=1):
    points=(a,b,c)
    return (points,tuple((p[0]/64,p[2]/64) for p in points),0x7fff,texture)
def square(x,z,size=64):
    a,b,c,d=(x,0,z),(x+size,0,z),(x+size,0,z+size),(x,0,z+size)
    return [face(a,b,c),face(a,c,d)]
def area(faces):
    return sum(abs(cross(sub(v[1],v[0]),sub(v[2],v[0]))[1]) for v,*_ in faces)
class CoverageTests(unittest.TestCase):
    def test_coplanar_grid_retains_outline_area_and_uv(self):
        original=sum((square(x,z) for x in (0,64) for z in (0,64)),[])
        merged=dissolve(original)
        self.assertEqual(len(merged),2)
        self.assertEqual(area(original),area(merged))
        self.assertEqual({v for f in merged for v in f[0]},{(0,0,0),(128,0,0),(128,0,128),(0,0,128)})
        for vertices,uv,*_ in merged:
            self.assertEqual(uv,tuple((p[0]/64,p[2]/64) for p in vertices))
    def test_courtyard_hole_is_not_filled(self):
        original=sum((square(x*64,z*64) for x in range(3) for z in range(3) if (x,z)!=(1,1)),[])
        merged=dissolve(original)
        self.assertEqual(area(original),area(merged))
        self.assertEqual(len(original),len(merged))
    def test_material_boundary_and_uv_seam_are_preserved(self):
        original=square(0,0)+square(64,0)
        vertices,uv,color,texture=original[-1]
        original[-1]=(vertices,tuple((u+.25,v) for u,v in uv),color,texture)
        self.assertEqual(len(dissolve(original)),len(original))
    def test_concave_outline_preserves_area(self):
        original=square(0,0)+square(64,0)+square(0,64)
        merged=dissolve(original)
        self.assertEqual(area(original),area(merged))
        self.assertEqual(len(merged),4)
if __name__=='__main__':unittest.main()
