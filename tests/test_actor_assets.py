import sys,struct,tempfile,unittest
from pathlib import Path
import numpy as np
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from stage_animation import pose
from actor_texture import SharedVehicleAtlas,PedestrianAtlas

class ActorAssetTests(unittest.TestCase):
    def test_named_root_does_not_animate_clump_wrapper(self):
        vertices=np.array([[0.,0.,0.],[1.,0.,0.],[0.,1.,0.]])
        weights=np.zeros((3,4));weights[:,0]=1
        model=(vertices,[(0,1,2)],[np.eye(4),np.eye(4)],[-1,0],[-1,0],['root','root'],[1],
               np.array([np.eye(4)]),np.zeros((3,4),dtype=int),weights,0.)
        track=np.array([[0.,0.,0.,1.,2.,3.,4.,0.]])
        named=pose(model,{'root':track},0)
        np.testing.assert_allclose(named,pose(model,{0:track},0))
        np.testing.assert_allclose(named,vertices+[2,3,4])

    def test_shared_atlas_deduplication_gutters_and_bounds(self):
        atlas=SharedVehicleAtlas();paint=((255,255,255),(255,255,255))
        source=Image.new('RGB',(128,128),'red');source.paste('blue',(64,0,128,128))
        slot=atlas.add(((255,255,255,255),'one'),{'one':source},paint)
        self.assertEqual(slot,atlas.add(((255,255,255,255),'alias'),{'alias':source},paint))
        solid=atlas.add(((24,24,24,255),''),{},paint)
        atlas.freeze();x,y,w,h=atlas.rects[slot]
        self.assertEqual(atlas.rects[solid][2:],(1,1))
        with self.assertRaises(ValueError):atlas.add(((255,0,0,255),''),{},paint)
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'atlas.atx';atlas.save(path);raw=path.read_bytes()
            self.assertEqual(struct.unpack_from('<4sHH',raw),(b'ATX1',256,128))
            self.assertEqual(len(raw),520+32768)
            pixels=np.frombuffer(raw[520:],'u1').reshape(128,256)
            np.testing.assert_equal(pixels[y-1,x:x+w],pixels[y,x:x+w])
            np.testing.assert_equal(pixels[y:y+h,x-1],pixels[y:y+h,x])
        uv=atlas.uv(slot,[[-1,-1],[2,2]])/16
        self.assertTrue(np.all(uv>=[x,y]) and np.all(uv<[x+w,y+h]))

    def test_pedestrian_atlas_tiles_do_not_overlap(self):
        atlas=PedestrianAtlas()
        for i in range(6):
            slot=atlas.add(Image.new('RGB',(128,256),(i*30,100,200)))
            uv=atlas.uv(slot,[[0,0],[1,1]])/16
            self.assertTrue(np.all(uv>=[i%4*32,i//4*64]))
            self.assertTrue(np.all(uv<[(i%4+1)*32,(i//4+1)*64]))

if __name__=='__main__':unittest.main()
