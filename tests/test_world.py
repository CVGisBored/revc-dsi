import sys
from pathlib import Path
import unittest
import math
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from stage_world import cells


def area(tri):
    a,b,c=tri
    u=[b[k]-a[k] for k in range(3)];v=[c[k]-a[k] for k in range(3)]
    n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
    return math.sqrt(sum(x*x for x in n))/2


class ChunkTests(unittest.TestCase):
    def test_seams_preserve_surface_area(self):
        # Crosses negative and positive X/Z boundaries with sloped elevation.
        triangle=[(-70,10,-70),(100,38,-20),(30,24,100)]
        pieces=list(cells(triangle))
        self.assertGreater(len(pieces),4)
        self.assertAlmostEqual(sum(area(p) for *_,p in pieces),area(triangle),places=6)
        for x,y,z,tri in pieces:
            for p in tri:
                for v,cell in zip(p,(x,y,z)):
                    self.assertGreaterEqual(v-cell*64,-1e-8)
                    self.assertLessEqual(v-cell*64,64+1e-8)

    def test_tall_geometry_is_split_vertically(self):
        triangle=[(0,27,0),(10,650,10),(20,27,0)]
        pieces=list(cells(triangle))
        self.assertGreater(max(y for x,y,z,p in pieces),8)
        self.assertAlmostEqual(sum(area(p) for *_,p in pieces),area(triangle),places=6)

    def test_coplanar_boundaries_not_duplicated(self):
        triangle=[(64,64,64),(70,64,64),(64,64,70)]
        pieces=list(cells(triangle))
        self.assertEqual(len(pieces),1)
        self.assertEqual(pieces[0][:3],(1,1,1))
