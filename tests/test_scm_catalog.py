"""Boundary tests for the offline SCM reader, independent of retail assets."""
import struct,sys,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import scm_cutscenes as scm
class CatalogueReaderTests(unittest.TestCase):
 def read(self,body,meta):
  with patch.object(scm,'inspect',return_value={'offsets':[0],'main_bytes':0}),patch.object(scm,'command_metadata',return_value=meta):
   return list(scm.commands(body,0))
 def test_last_mission_reads_to_eof(self):
  self.assertEqual(self.read(struct.pack('<H',17),{17:('RETURN',[])}),[(0,'RETURN',[])])
 def test_script_arguments_do_not_become_opcodes(self):
  body=struct.pack('<HBi',10,1,1234)+struct.pack('<BbBH',4,-2,2,44)+b'\0'+struct.pack('<H',17)
  self.assertEqual(self.read(body,{10:('START_NEW_SCRIPT',['ARGTYPE_LABEL']),17:('RETURN',[])})[1],(13,'RETURN',[]))
 def test_truncated_variable_arguments_fail(self):
  with self.assertRaises((IndexError,struct.error)):
   self.read(struct.pack('<HBi',10,1,1234)+b'\x06',{10:('START_NEW_SCRIPT',['ARGTYPE_LABEL'])})
if __name__=='__main__':unittest.main()
