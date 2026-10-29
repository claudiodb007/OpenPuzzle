"""Full-size assignments through the real adapter; synthetic native protocol only."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

DRIVER=Path(sys.argv.pop(1)).resolve()
ADAPTER=Path(sys.argv.pop(1)).resolve()
START=2**64
LIMIT=2**40
BATCH=4096*256*1024
COUNT=LIMIT+32*BATCH

ENGINE='''#!/usr/bin/env python3
import os,signal,sys,time
from pathlib import Path
if sys.argv[1:]==['--identity']:
 print('OpenPuzzle-VanityRange 0.2.0 CUDA target protocol 1');sys.exit()
_,gate,start,count,grid,targets,output,mode,owner=sys.argv
assert gate=='--target-file' and int(count)<=2**40
stopped=False
def stop(*_):
 global stopped;stopped=True
signal.signal(signal.SIGTERM,stop)
Path(output).write_bytes(b'');os.chmod(output,0o600)
batch=int(grid)*256*1024;count=int(count);checked=0
print('OPV1 BEGIN',start,count,batch,flush=True)
while checked<count:
 checked+=min(batch,count-checked)
 print('OPV1 PROGRESS',checked,flush=True)
 if os.environ.get('OPV_SEGMENT_HOLD') and int(start,16)>2**64:
  while not stopped:time.sleep(.01)
 if stopped:
  print('OPV1 STOPPED',checked,flush=True);sys.exit(130)
print('OPV1 COMPLETE',checked,flush=True)
'''

class Tests(unittest.TestCase):
 def setUp(self):
  os.umask(0o077);self.tmp=tempfile.TemporaryDirectory(prefix='opv-duration-');self.root=Path(self.tmp.name)
  self.workspace=self.root/'assignment';self.workspace.mkdir();self.controls=self.workspace/'vanity-controls';self.controls.mkdir()
  self.engine=self.root/'engine';self.engine.write_text(ENGINE);self.engine.chmod(0o700)
  self.target='1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH'
  self.targets=self.workspace/'vanity-targets.txt';self.targets.write_text('OPV-TARGETS-1\np2pkh '+self.target+'\n')
  sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
  text=f'OPV-COMMON-1\n{START:064x} {COUNT} 4096\n'+json.dumps(self.target)+' '+json.dumps(str(self.engine))+' '+sha(self.engine)+' '+json.dumps(str(ADAPTER))+' '+sha(ADAPTER)+'\n'
  (self.workspace/'vanity-binding.txt').write_text(text)
  self.binding=hashlib.sha256(text.encode()).hexdigest();self.process=None
 def tearDown(self):
  if self.process and self.process.poll() is None:
   self.process.terminate();self.process.wait(timeout=5)
  self.tmp.cleanup()
 def start(self,hold=False):
  env=os.environ.copy();env.pop('OPV_SEGMENT_HOLD',None)
  if hold:env['OPV_SEGMENT_HOLD']='1'
  self.log=(self.root/'adapter.log').open('w')
  self.process=subprocess.Popen([str(ADAPTER),str(self.engine),str(self.workspace/'vanity-native'),f'{START:x}',str(COUNT),'4096',str(self.targets),'stop',self.binding,str(self.controls),'--targets-only','--common-runtime','--owner-parent'],env=env,stdout=self.log,stderr=subprocess.STDOUT)
 def finish(self,code=0):
  self.assertEqual(self.process.wait(timeout=30),code,(self.root/'adapter.log').read_text());self.log.close()
 def wait_second_segment(self):
  cp=self.workspace/'vanity-native/checkpoint.txt';deadline=time.monotonic()+15
  while time.monotonic()<deadline:
   if cp.exists() and int(cp.read_text().split()[6])>LIMIT:return
   if self.process.poll() is not None:self.fail((self.root/'adapter.log').read_text())
   time.sleep(.01)
  self.fail('second native segment did not start')
 def proof(self):
  r=subprocess.run([str(DRIVER),'inspect',str(self.workspace),str(COUNT)],capture_output=True,text=True,timeout=20)
  self.assertEqual(r.returncode,0,r.stderr);self.assertTrue(json.loads(r.stdout)['proof'],r.stdout)
  spans=[]
  for path in (self.workspace/'vanity-native').glob('attempt-*/engine.log'):
   rows=[line.split() for line in path.read_text().splitlines() if line.startswith('OPV1 ')]
   self.assertLessEqual(int(rows[0][3]),LIMIT)
   spans.append((int(rows[0][2],16),int(rows[-1][2])))
  checked=START
  for first,count in sorted(spans):self.assertEqual(first,checked);checked+=count
  self.assertEqual(checked,START+COUNT)
 def test_size_matches_speed_and_duration(self):
  subprocess.run([str(DRIVER),'duration'],check=True,timeout=10)
 def test_one_assignment_spans_native_limit_with_exact_proof(self):
  self.start();self.finish();self.proof()
 def test_stop_resume_across_native_boundary_preserves_attempts(self):
  self.start(True);self.wait_second_segment();(self.controls/'cancel').touch();self.finish(130)
  old={p:p.read_bytes() for p in (self.workspace/'vanity-native').glob('attempt-*/engine.log')}
  (self.controls/'cancel').unlink();self.start();self.finish();self.proof()
  for path,data in old.items():self.assertEqual(path.read_bytes(),data)
 def test_safe_stop_finishes_all_native_segments(self):
  (self.controls/'safe-stop').touch();self.start();self.finish();self.proof()
  self.assertEqual((self.workspace/'vanity-native/checkpoint.txt').read_text().split()[8],'1')

if __name__=='__main__':unittest.main(verbosity=2)
