"""Ordinary run/status/stop/safestop, actual CLI/core; synthetic engine and HTTP/GPU fixtures."""
import hashlib,json,os,shutil,signal,subprocess,sys,tempfile,threading,time,unittest,uuid
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
CLI=Path(sys.argv.pop(1)).resolve();ADAPTER=Path(sys.argv.pop(1)).resolve()
sys.path.insert(0,str(Path(__file__).parent/'support'))
import Support as s
class Tests(unittest.TestCase):
 def setUp(self):
  os.umask(0o077);self.tmp=tempfile.TemporaryDirectory(prefix="opv ordinary ç ' $() ");self.root=Path(self.tmp.name);self.home=self.root/'home';self.home.mkdir()
  self.bin=self.root/'bin';self.bin.mkdir();self.calls=[];self.reject=False;self.bad_range=False;self.complete_fail=False;self.processes=[];self.checked=0
  shutil.copytree(Path(__file__).parent/'support',self.root/'support');self.engine=self.root/'FakeTargetEngine.py'
  self.engine.write_text((Path(__file__).parent/'FakeTargetEngine.py').read_text());self.engine.chmod(0o700)
  self.engine_name='vanity';self.uuid='GPU-12345678-1234-1234-1234-123456789abc';self.assignment=str(uuid.uuid4());self.client=str(uuid.uuid4());self.count=2**30;self.first=s.BASE;self.target=s.address(s.key_hash(s.BASE-2))
  self.evidence=self.root/'measured-proof';self.evidence.write_text('fixture measurement pin\n')
  self.profile=self.root/'run-profile';self.write_profile()
  self.puzzles=self.root/'puzzles';self.puzzles.mkdir();self.write_puzzle()
  smi=self.bin/'nvidia-smi';smi.write_text('''#!/usr/bin/env python3
import sys
args=' '.join(sys.argv)
if 'index,uuid,name,driver_version,power.limit,temperature.gpu' in args: print('0, GPU-12345678-1234-1234-1234-123456789abc, Fixture GPU, fixture-driver, 220, 40')
elif 'index,name,uuid,memory.total,driver_version' in args:print('0, Fixture GPU, GPU-12345678-1234-1234-1234-123456789abc, 12000, fixture-driver')
elif 'index,pci.bus_id,temperature.gpu,power.draw,power.limit' in args:print('0, 00000000:01:00.0, 40, 100, 220')
elif '--query-compute-apps' in args:pass
else:sys.exit(1)
''');smi.chmod(0o700)
  test=self
  class Handler(BaseHTTPRequestHandler):
   def log_message(self,*_):pass
   def do_POST(self):
    q=json.loads(self.rfile.read(int(self.headers['Content-Length'])));test.calls.append((self.path,q));code=200
    if self.path=='/api/range/claim':
     test.assertEqual(q['client_id'],test.client);test.assertEqual(q['puzzle'],69);test.assertEqual(q['backend'],'cuda');test.assertEqual(q['requested_keys'],2**36)
     result=dict(assignment_id=test.assignment,puzzle=69,range_id=37,target=test.target,start=f'{test.first:x}',end=f'{test.first+test.count-1:x}',search_mode='linear')
     if test.bad_range:result['end']=f'{test.first+2**39:x}'
    elif self.path=='/api/range/progress':
     test.assertEqual(q['assignment_id'],test.assignment);test.assertEqual(q['client_id'],test.client);test.assertGreaterEqual(int(q['keys_checked']),test.checked);test.checked=int(q['keys_checked'])
     result=dict(success=True,status='running')
     if test.reject:result=dict(error='assignment_lease_expired',message='Lease expired');code=409
    elif self.path=='/api/range/complete':
     test.assertEqual(q['assignment_id'],test.assignment);test.assertEqual(q['status'],'completed');test.assertEqual(q['exit_code'],0);test.assertEqual(int(q['keys_checked']),test.count);result=dict(success=True,status='completed',already_finalized=True)
     if test.complete_fail:result=dict(error='temporary_unavailable');code=503
    elif self.path in ('/api/client/register','/api/client/heartbeat'):result=dict(success=True)
    else:result=dict(error='unexpected_endpoint');code=404
    data=json.dumps(result).encode();self.send_response(code);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
  self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler);self.server.daemon_threads=True;self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start();self.url=f'http://127.0.0.1:{self.server.server_port}'
  self.env={k:v for k,v in os.environ.items() if not k.startswith(('OPENPUZZLE_','OPV_','CUDA_','GIT_'))};self.env.update(HOME=str(self.home),XDG_CONFIG_HOME=str(self.home/'.config'),XDG_DATA_HOME=str(self.home/'.local/share'),OPENPUZZLE_VANITY_COMMON_PROFILE=str(self.profile),OPENPUZZLE_SERVER_URL=self.url,OPENPUZZLE_PUZZLE_DIR=str(self.puzzles),PYTHONDONTWRITEBYTECODE='1',PATH=str(self.bin)+':'+os.environ['PATH'])
  config=self.home/'.config/OpenPuzzle';config.mkdir(parents=True);(config/'client.id').write_text(self.client+'\n')
 def tearDown(self):
  for p in self.processes:
   if p.poll() is None:p.terminate()
  for p in self.processes:
   try:p.wait(timeout=5)
   except subprocess.TimeoutExpired:p.kill();p.wait()
  self.server.shutdown();self.server.server_close();self.thread.join(timeout=5);self.tmp.cleanup()
 def write_profile(self):
  def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
  quote=lambda x:'"'+str(x).replace('\\','\\\\').replace('"','\\"')+'"'
  self.profile.write_text('OPV-RUN-PROFILE-1\n'+sha(CLI)+' local-only\n'+quote(self.engine)+' '+quote(ADAPTER)+' '+sha(self.engine)+' '+sha(ADAPTER)+'\n'+quote(self.uuid)+' "Fixture GPU" "fixture-driver" 128 2500 220\n1\n'+quote(self.evidence)+' '+sha(self.evidence)+'\n');self.profile.chmod(0o600)
 def write_puzzle(self):
  (self.puzzles/'69.json').write_text(json.dumps(dict(number=69,address=self.target,keyspace=f'{self.first:x}:{2**69-1:x}',search_mode='linear')))
 def args(self,*extra):return ['run','69','--engine',self.engine_name,'--backend','cuda','--device','0','--duration-minutes','2',*extra]
 def command(self,args,code=0,env=None):
  r=subprocess.run([str(CLI),*args],env=env or self.env,capture_output=True,text=True,timeout=25);self.assertEqual(r.returncode,code,r.stdout+r.stderr+self.diagnostic());return r
 def start(self,*extra):
  log=(self.root/f'cli-{len(self.processes)}.log').open('w');p=subprocess.Popen([str(CLI),*self.args(*extra)],env=self.env,stdout=log,stderr=subprocess.STDOUT);log.close();self.processes.append(p);return p
 def diagnostic(self):return '\n'.join(p.read_text()[-3000:] for p in self.root.glob('cli-*.log'))
 @property
 def workspace(self):return self.home/'.local/share/OpenPuzzle/assignments'/self.assignment
 @property
 def state(self):return self.home/'.local/share/OpenPuzzle/client.state'
 def wait_progress(self):
  deadline=time.monotonic()+12
  while time.monotonic()<deadline:
   cp=self.workspace/'vanity-native/checkpoint.txt'
   if cp.is_file() and int(cp.read_text().split()[6])>0:return
   if self.processes[-1].poll() is not None:self.fail(self.diagnostic())
   time.sleep(.01)
  self.fail(self.diagnostic())
 def wait(self,p,code=0):self.assertEqual(p.wait(timeout=20),code,self.diagnostic())
 def snapshot(self):return {str(p.relative_to(self.workspace)):hashlib.sha256(p.read_bytes()).hexdigest() for p in self.workspace.rglob('attempt-*/engine.log')}
 def test_ordinary_once_uses_capped_existing_api_and_common_completion(self):
  self.engine_name='VANITY';p=self.start('--once');self.wait(p);self.assertEqual(sum(x=='/api/range/claim' for x,_ in self.calls),1);self.assertEqual(sum(x=='/api/range/complete' for x,_ in self.calls),1)
  self.assertIn('Engine............. Vanity',self.diagnostic());self.assertTrue((self.workspace/'vanity-run-profile.txt').is_file());self.assertFalse(self.state.exists())
 def pause(self):
  p=self.start('--once');self.wait_progress();self.command(['stop']);self.wait(p,130)
  self.assertEqual(sum(x=='/api/range/complete' for x,_ in self.calls),0);self.assertTrue(self.state.exists());self.assertIn('stopped',self.command(['status']).stdout)
 def test_stop_reopen_resume_same_assignment_and_preserve_native_attempts(self):
  self.pause();before=self.snapshot();p=self.start('--once');self.wait(p);self.assertEqual(sum(x=='/api/range/claim' for x,_ in self.calls),1)
  for name,digest in before.items():self.assertEqual(hashlib.sha256((self.workspace/name).read_bytes()).hexdigest(),digest)
  self.assertEqual(sum(x=='/api/range/complete' for x,_ in self.calls),1)
 def test_safe_stop_finishes_assignment_then_blocks_next_claim(self):
  p=self.start();self.wait_progress();self.command(['safestop']);self.wait(p);self.assertEqual(sum(x=='/api/range/claim' for x,_ in self.calls),1);self.assertEqual(sum(x=='/api/range/complete' for x,_ in self.calls),1)
 def test_refused_lease_keeps_checkpoint_no_claim_no_launch(self):
  self.pause();before=self.snapshot();self.reject=True;self.command(self.args('--once'),1);self.assertEqual(before,self.snapshot());self.assertTrue(self.state.exists());self.assertEqual(sum(x=='/api/range/claim' for x,_ in self.calls),1)
 def test_recovery_dryrun_does_not_launch_or_contact_server(self):
  self.pause();before=self.snapshot();calls=len(self.calls);self.command(self.args('--dry-run'));self.assertEqual(before,self.snapshot());self.assertEqual(calls,len(self.calls));self.assertTrue(self.state.exists())
 def test_profile_changed_during_pause_blocks_before_http(self):
  self.pause();self.evidence.write_text('changed');calls=len(self.calls);self.command(self.args('--once'),1);self.assertEqual(calls,len(self.calls));self.assertTrue(self.state.exists())
 def test_remote_endpoint_wrong_geometry_and_capabilities_block_before_http(self):
  for args in [self.args('--server','https://example.invalid'),self.args('--threads','64'),self.args('--backend','opencl'),self.args('--with-cpu'),self.args('--devices','0,1')]:self.command(args,1)
  self.evidence.write_text('changed');self.command(self.args('--once'),1);self.assertEqual(self.calls,[]);self.assertFalse(self.workspace.exists())
 def test_lost_completion_reopen_uses_idempotent_complete_without_lease_or_kernel(self):
  self.complete_fail=True;p=self.start('--once');deadline=time.monotonic()+15
  while not any(path=='/api/range/complete' for path,_ in self.calls) and time.monotonic()<deadline:time.sleep(.02)
  self.assertTrue(any(path=='/api/range/complete' for path,_ in self.calls),self.diagnostic());p.kill();self.wait(p,-signal.SIGKILL)
  before=self.snapshot();self.complete_fail=False;self.reject=True;calls=len(self.calls);p=self.start('--once');self.wait(p)
  self.assertEqual(before,self.snapshot());self.assertEqual([path for path,_ in self.calls[calls:]],['/api/range/complete'])
  self.assertEqual(sum(path=='/api/range/claim' for path,_ in self.calls),1)
 def test_owner_process_loss_reopens_active_engine_without_duplicate_launch(self):
  p=self.start('--once');self.wait_progress();p.kill();self.wait(p,-signal.SIGKILL);p=self.start('--once');self.wait(p)
  self.assertEqual(sum(path=='/api/range/claim' for path,_ in self.calls),1)
  self.assertEqual(len(list((self.workspace/'vanity-native').glob('attempt-*'))),1)
 def test_wrong_gpu_driver_power_busy_and_hot_contexts_block_before_claim(self):
  smi=self.bin/'nvidia-smi';original=smi.read_text()
  for before,after in [('Fixture GPU','Other GPU'),('fixture-driver','changed-driver'),('220, 40','210, 40'),('220, 40','220, 90'),("elif '--query-compute-apps' in args:pass","elif '--query-compute-apps' in args:print('987, '+str('GPU-12345678-1234-1234-1234-123456789abc'))")]:
   smi.write_text(original.replace(before,after));self.command(self.args('--once'),1)
  self.assertEqual(self.calls,[]);self.assertFalse(self.workspace.exists())
 def test_api_assignment_above_native_budget_is_refused_without_launch(self):
  self.bad_range=True;self.command(self.args('--once'),1);self.assertFalse((self.workspace/'vanity-native').exists());self.assertEqual(sum(x=='/api/range/claim' for x,_ in self.calls),1)
if __name__=='__main__':unittest.main(verbosity=2)
