"""Two synthetic GPUs through the unchanged shared CUDA supervisor and range API."""
import hashlib,json,os,shutil,sqlite3,subprocess,sys,tempfile,threading,time,unittest,uuid
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
CLI=Path(sys.argv.pop(1)).resolve();ADAPTER=Path(sys.argv.pop(1)).resolve()

class Tests(unittest.TestCase):
 def setUp(self):
  os.umask(0o077);self.tmp=tempfile.TemporaryDirectory(prefix='opv-multi-');self.root=Path(self.tmp.name);self.home=self.root/'home';self.home.mkdir()
  self.calls=[];self.claims={};self.completions=[];self.final_statuses=[];self.process=None
  engines=self.root/'libexec/OpenPuzzle';engines.mkdir(parents=True)
  self.cli=self.root/'openpuzzle';shutil.copy2(CLI,self.cli);shutil.copy2(ADAPTER,engines/'openpuzzle-vanity-adapter')
  shutil.copytree(Path(__file__).parent/'support',engines/'support')
  source=(Path(__file__).parent/'FakeTargetEngine.py').read_text()
  source=source.replace("assert gate=='--target-file' and mode in ('stop','exhaust')", """assert gate=='--target-file' and mode in ('stop','exhaust')
import json
device=(int(start,16)-s.BASE)//(2**35)
assert os.environ['CUDA_VISIBLE_DEVICES']==f'GPU-12345678-1234-1234-1234-{device:012d}',os.environ['CUDA_VISIBLE_DEVICES']
with open(os.environ['OPV_MULTI_ROOT']+'/launches','a') as launch:launch.write(json.dumps(dict(device=device,start=start,count=int(count)))+'\\n')
""")
  source=source.replace('    time.sleep(.12)',"    while not Path(os.environ['OPV_MULTI_ROOT']+'/release').exists() and not stopped:time.sleep(.01)\n    time.sleep(.01)")
  (engines/'vanity-offline').write_text(source);(engines/'vanity-offline').chmod(0o700)
  (self.root/'nvidia-smi').write_text('''#!/usr/bin/env python3
import sys
args=' '.join(sys.argv)
if 'index,name,uuid,memory.total,driver_version' in args:
 for d in range(2):print(f'{d}, Fixture GPU {d}, GPU-12345678-1234-1234-1234-{d:012d}, 12000, fixture')
elif 'index,pci.bus_id,temperature.gpu,power.draw,power.limit' in args:
 for d in range(2):print(f'{d}, 00000000:0{d+1}:00.0, 40, 100, 220')
elif '--query-compute-apps' in args:pass
else:sys.exit(1)
''');(self.root/'nvidia-smi').chmod(0o700)
  self.env={k:v for k,v in os.environ.items() if not k.startswith(('OPENPUZZLE_','OPV_','CUDA_','GIT_'))}
  self.env.update(HOME=str(self.home),XDG_CONFIG_HOME=str(self.home/'.config'),XDG_DATA_HOME=str(self.home/'.local/share'),PATH=str(self.root)+':'+os.environ['PATH'],OPV_MULTI_ROOT=str(self.root),PYTHONDONTWRITEBYTECODE='1')
  (self.home/'.local/share/OpenPuzzle').mkdir(parents=True)
  self.command(['benchmark','--engine','vanity','--backend','cuda','--gpu','0'])
  dbpath=self.home/'.local/share/OpenPuzzle/openpuzzle.db'
  with sqlite3.connect(dbpath) as db:
   for d in range(2):db.execute('INSERT INTO gpu_profiles(gpu_name,backend,engine,blocks,threads,points,average_speed) VALUES(?,?,?,?,?,?,?)',(f'Fixture GPU {d}','CUDA','Vanity',1024,256,1024,2500+d*100))
  sys.path.insert(0,str(engines/'support'));import Support as s
  self.first=s.BASE;self.count=2**30;self.target=s.address(s.key_hash(s.BASE-2))
  puzzles=self.root/'puzzles';puzzles.mkdir();(puzzles/'71.json').write_text(json.dumps(dict(number=71,address=self.target,keyspace=f'{s.BASE:x}:{2**71-1:x}',search_mode='linear')))
  self.env['OPENPUZZLE_PUZZLE_DIR']=str(puzzles)
  test=self
  class Handler(BaseHTTPRequestHandler):
   def log_message(self,*_):pass
   def do_POST(self):
    q=json.loads(self.rfile.read(int(self.headers['Content-Length'])));test.calls.append((self.path,q))
    if self.path=='/api/range/claim':
     slot=q['execution_slot'];d=int(slot.split('-')[1]);test.assertIn(d,(0,1));test.assertEqual(q['target_duration_minutes'],60);test.assertEqual(q['requested_keys'],(2500+d*100)*1000000*60*60)
     test.assertNotIn(slot,test.claims);identifier=str(uuid.uuid4());test.claims[slot]=identifier;first=test.first+d*2**35
     result=dict(assignment_id=identifier,puzzle=71,range_id=80+d,target=test.target,start=f'{first:x}',end=f'{first+test.count-1:x}',search_mode='linear')
    elif self.path=='/api/range/complete':
     test.assertIn(q['assignment_id'],test.claims.values());test.completions.append(q['assignment_id']);test.final_statuses.append(q['status'])
     if q['status']=='completed':test.assertEqual(int(q['keys_checked']),test.count);test.assertEqual(q['exit_code'],0)
     else:test.assertEqual(q['status'],'cancelled');test.assertEqual(q['exit_code'],-2);test.assertLessEqual(int(q['keys_checked']),test.count)
     result=dict(success=True,status=q['status'])
    elif self.path=='/api/client/heartbeat':
     vanity=[entry for entry in q['engines'] if entry['name']=='Vanity'];test.assertEqual(len(vanity),1);test.assertEqual(vanity[0],dict(name='Vanity',backend='CUDA',installed=True,available=True));result=dict(success=True,status='running')
    else:result=dict(success=True,status='running')
    data=json.dumps(result).encode();self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
  self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler);self.server.daemon_threads=True;threading.Thread(target=self.server.serve_forever,daemon=True).start()
  self.env['OPENPUZZLE_SERVER_URL']=f'http://127.0.0.1:{self.server.server_port}'
 def tearDown(self):
  (self.root/'release').touch()
  if self.process and self.process.poll() is None:
   try:self.command(['stop'])
   except Exception:pass
   try:self.process.wait(timeout=15)
   except subprocess.TimeoutExpired:self.process.kill();self.process.wait()
  if hasattr(self,'server'):self.server.shutdown();self.server.server_close()
  self.tmp.cleanup()
 def command(self,args):
  r=subprocess.run([str(self.cli),*args],env=self.env,capture_output=True,text=True,timeout=25);self.assertEqual(r.returncode,0,r.stdout+r.stderr);return r.stdout
 def test_shared_supervisor_two_slots_physical_binding_and_global_safe_stop(self):
  self.run_supervisor('safestop')
 def test_shared_supervisor_global_stop_cancels_both_assignments(self):
  self.run_supervisor('stop')
 def run_supervisor(self,control):
  with (self.root/'runtime.log').open('w') as log:
   self.process=subprocess.Popen([str(self.cli),'run','71','--engine','vanity','--backend','cuda','--devices','all'],env=self.env,stdout=log,stderr=subprocess.STDOUT)
  data=self.home/'.local/share/OpenPuzzle'
  deadline=time.monotonic()+35
  while time.monotonic()<deadline:
   launches=self.root/'launches'
   if launches.exists() and len(launches.read_text().splitlines())==2 and len(list(data.glob('client-cuda-*.state')))==2:break
   if self.process.poll() is not None:self.fail((self.root/'runtime.log').read_text())
   time.sleep(.02)
  else:self.fail((self.root/'runtime.log').read_text())
  self.assertEqual(len(list(data.glob('client-cuda-*.state'))),2)
  self.command([control])
  if control=='safestop':(self.root/'release').touch()
  self.assertEqual(self.process.wait(timeout=30),0,(self.root/'runtime.log').read_text())
  self.assertEqual(set(self.claims),{'cuda-0','cuda-1'});self.assertCountEqual(self.completions,list(self.claims.values()))
  self.assertEqual(self.final_statuses,['completed' if control=='safestop' else 'cancelled']*2)
  rows=[json.loads(row) for row in (self.root/'launches').read_text().splitlines()];self.assertEqual({row['device'] for row in rows},{0,1})

if __name__=='__main__':unittest.main(verbosity=2)
