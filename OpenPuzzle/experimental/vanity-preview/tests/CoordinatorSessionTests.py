#!/usr/bin/env python3
"""Native HttpRangeClient, native adapter, loopback server and synthetic GPU engine."""
import hashlib,http.server,json,os,signal,subprocess,sys,tempfile,threading,time,unittest,uuid
from pathlib import Path
CLI=Path(sys.argv.pop(1)).resolve();ADAPTER=Path(sys.argv.pop(1)).resolve()
sys.path.insert(0,str(Path(__file__).parent/'support'))
import Support as s
UUID='GPU-11111111-2222-3333-4444-555555555555'
ENGINE=Path(__file__).with_name('FakeTargetEngine.py').resolve()
BATCH=256*1024

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
class Fixture:
 def __init__(self,root,kind='normal',engine=ENGINE,count=5*BATCH+7,grid=1,segment=2*BATCH,target=None,planning=2519.41):
  self.root=Path(root);self.planning=float(planning);self.client_id="opv-coord-"+str(uuid.uuid4());self.kind=kind;self.engine=Path(engine);self.count=count;self.grid=grid;self.segment=segment;self.target=target or s.address(s.key_hash(s.BASE-1));self.requests=[];self.claims=0;self.complete_attempts=0;self.completions=0;self.progress=[];self.errors=[];self.reports=0;self.seen={};outer=self
  class Handler(http.server.BaseHTTPRequestHandler):
   protocol_version='HTTP/1.1'
   def log_message(self,*_):pass
   def do_POST(self):
    try:
     body=json.loads(self.rfile.read(int(self.headers['Content-Length'])));outer.requests.append(dict(path=self.path,payload=body));code,response=outer.handle(self.path,body)
    except Exception as error:outer.errors.append(str(error));code,response=500,dict(error='fixture_error')
    data=json.dumps(response,separators=(',',':')).encode();self.send_response(code);self.send_header('Content-Length',str(len(data)));self.send_header('Connection','close');self.end_headers();self.wfile.write(data);self.close_connection=True
  self.server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);self.server.daemon_threads=True
  self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start();self.url='http://127.0.0.1:'+str(self.server.server_port)
 def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
 def handle(self,path,b):
  self.seen[path]=self.seen.get(path,0)+1
  if path=='/api/client/register':
   assert b==dict(client_id=self.client_id);return 200,dict(success=True)
  if path=='/api/client/heartbeat':
   assert b['client_id']==self.client_id and b['engines']==[dict(name='Vanity',backend='cuda',installed=True,available=True)]
   return 200,dict(success=True)
  if path=='/api/range/claim':
   self.claims+=1;assert self.claims==1
   assert b['client_id']==self.client_id and b['puzzle']==20 and b['backend']=='cuda' and b['target_duration_minutes']==60 and abs(b['speed_mkeys']-self.planning)<.000001
   if self.kind=='unavailable':return 200,dict(available=False,reason='No work')
   if self.kind=='ambiguous':return 503,dict(error='temporary_unavailable')
   if self.kind=='redirect':return 302,dict(error='redirect')
   a=dict(assignment_id='coordinator-assignment-A',puzzle=20,range_id=37,target=self.target,search_mode='linear',required_backend='cuda',start=f'{s.BASE:064x}',end=f'{s.BASE+self.count-1:064x}')
   if self.kind=='wrong-target':a['target']=s.address(s.key_hash(s.BASE))
   if self.kind=='outside-scope':a['end']=f'{s.BASE+self.count:064x}'
   return 200,a
  assert b['client_id']==self.client_id and b['assignment_id']=='coordinator-assignment-A'
  if path=='/api/range/progress':
   if self.kind=='bad-ack':return 200,dict(success=False)
   n=int(b['keys_checked']);assert 0<=n<=self.count and (not self.progress or n>=self.progress[-1])
   if self.kind=='lease-before' or self.kind=='lease-during' and n>0:return 409,dict(error='assignment_lease_expired')
   if self.kind=='progress-fail' and n>0:return 503,dict(error='temporary_unavailable')
   if self.kind=='progress-once' and n>0 and not getattr(self,'failed_once',False):self.failed_once=True;return 503,dict(error='temporary_unavailable')
   self.progress.append(n)
   if self.kind in ('cancel','safe') and n>0:
    (self.root/'controls'/('cancel' if self.kind=='cancel' else 'safe-stop')).touch(mode=0o600)
   return 200,dict(success=True)
  if path=='/api/range/complete':
   self.complete_attempts+=1;assert int(b['keys_checked'])==self.count and self.progress[-1]==self.count and b['status']=='completed' and b['exit_code']==0
   if self.kind=='completion-fail':return 503,dict(error='temporary_unavailable')
   if self.kind=='completion-once' and self.complete_attempts==1:return 503,dict(error='temporary_unavailable')
   self.completions+=1;assert self.completions==1;return 200,dict(success=True)
  if path=='/api/puzzle/report-solution':self.reports+=1;assert set(b)=={'assignment_id','client_id'};return 200,dict(success=True)
  raise AssertionError(path)
 def argv(self,resume=False):
  options=dict(server=self.url,root=str(self.root),controls=str(self.root/'controls'),engine=str(self.engine),engine_sha256=sha(self.engine),adapter=str(ADAPTER),adapter_sha256=sha(ADAPTER),client_id=self.client_id,target=self.target,puzzle='20',grid=str(self.grid),segment_keys=str(self.segment),scope_start=f'{s.BASE:064x}',scope_end=f'{s.BASE+self.count-1:064x}',gpu_uuid=UUID,speed_mkeys=format(self.planning,'.12f'))
  args=[str(CLI),'vanity-preview','coordinator-test','--enable-experimental-vanity-local']
  for k,v in options.items():args+=['--'+k.replace('_','-'),v]
  if resume:args+=['--resume']
  return args
 def env(self):return dict(os.environ,CUDA_VISIBLE_DEVICES=UUID,HOME=str(self.root/'home'),XDG_CONFIG_HOME=str(self.root/'home/.config'),XDG_DATA_HOME=str(self.root/'home/.local/share'),OPENPUZZLE_EXECUTION_SLOT='cuda-0')
 def state(self):return (self.root/'coordinator-state.txt').read_text().splitlines()[2].split()
 def kernels(self):return list(self.root.glob('segment-*/attempt-*/engine.log'))
class Tests(unittest.TestCase):
 def setUp(self):os.umask(0o077);self.tmp=tempfile.TemporaryDirectory(prefix="opv coordinator ç ' ");self.directory=Path(self.tmp.name);self.fixtures=[]
 def tearDown(self):
  for f in self.fixtures:f.close()
  self.tmp.cleanup()
 def fixture(self,kind='normal',**kwargs):
  root=self.directory/str(len(self.fixtures));root.mkdir();(root/'home').mkdir();f=Fixture(root,kind,**kwargs);self.fixtures.append(f);return f
 def run_cli(self,f,code=0,resume=False,args=None):
  result=subprocess.run(args or f.argv(resume),env=f.env(),text=True,capture_output=True,timeout=25)
  self.assertEqual(result.returncode,code,result.stdout+'\n'+result.stderr);self.assertFalse(f.errors,f.errors);return result
 def test_normal_assigned_range_split_and_exact_tail(self):
  f=self.fixture();r=self.run_cli(f);self.assertEqual(f.state()[:4],['done',str(f.count),str(f.count),'3']);self.assertEqual(f.claims,1);self.assertEqual(f.completions,1);self.assertEqual(len(f.kernels()),3);self.assertIn('next_claim_blocked=1',r.stdout);self.run_cli(f,20);self.run_cli(f,20,True);self.assertEqual(f.claims,1)
 def test_unavailable_no_kernel(self):f=self.fixture('unavailable');self.run_cli(f);self.assertEqual(f.state()[0],'unavailable');self.assertFalse(f.kernels())
 def test_ambiguous_claim_blocks_retry(self):f=self.fixture('ambiguous');self.run_cli(f,2);self.run_cli(f,20,True);self.assertEqual(f.claims,1);self.assertFalse(f.kernels())
 def test_wrong_target_and_scope_block_before_kernel(self):
  for kind in ('wrong-target','outside-scope'):
   f=self.fixture(kind);self.run_cli(f,2);self.assertFalse(f.kernels());self.assertEqual(f.completions,0)
 def test_unacknowledged_progress_blocks_kernel_and_can_retry(self):
  f=self.fixture('bad-ack');self.run_cli(f,2);self.assertFalse(f.kernels());self.assertEqual(f.state()[0],'paused');f.kind='normal';self.run_cli(f,0,True);self.assertEqual(f.claims,1)
 def test_redirect_is_not_followed_and_claim_stays_ambiguous(self):
  f=self.fixture('redirect');self.run_cli(f,2);self.assertEqual(f.state()[0],'claiming');self.assertFalse(f.kernels());self.run_cli(f,20,True);self.assertEqual(f.claims,1)
 def test_transient_progress_retry(self):f=self.fixture('progress-once');self.run_cli(f);self.assertTrue(f.failed_once);self.assertEqual(f.completions,1)
 def test_progress_failure_resume_same_assignment(self):
  f=self.fixture('progress-fail');self.run_cli(f,3);self.assertEqual(f.state()[0],'paused');f.kind='normal';self.run_cli(f,0,True);self.assertEqual(f.claims,1);self.assertEqual(f.completions,1)
 def test_lease_refused_before_and_during_engine(self):
  for kind in ('lease-before','lease-during'):
   f=self.fixture(kind);self.run_cli(f,2);self.assertEqual(f.state()[0],'rejected');self.run_cli(f,20,True);self.assertEqual(f.completions,0)
   if kind=='lease-before':self.assertFalse(f.kernels())
 def test_cancel_and_resume_without_new_claim(self):
  f=self.fixture('cancel');self.run_cli(f,130);first=int(f.state()[2]);self.assertTrue(0<first<f.count);f.kind='normal';self.run_cli(f,0,True);self.assertEqual(f.claims,1);self.assertEqual(f.completions,1);self.assertGreaterEqual(int(f.state()[3]),3)
 def test_safe_stop_finishes_entire_assignment_across_segments(self):
  f=self.fixture('safe');self.run_cli(f);self.assertEqual(f.completions,1);self.assertEqual(len(f.kernels()),3);self.run_cli(f,20);self.assertEqual(f.claims,1)
 def test_pending_completion_retry_without_relaunch(self):
  f=self.fixture('completion-fail');self.run_cli(f,1);self.assertEqual(f.state()[0],'pending-complete');launches=len(f.kernels());f.kind='normal';self.run_cli(f,0,True);self.assertEqual(len(f.kernels()),launches);self.assertEqual(f.claims,1)
 def test_single_transient_completion_retry(self):f=self.fixture('completion-once');self.run_cli(f);self.assertEqual(f.complete_attempts,2);self.assertEqual(f.completions,1)
 def test_changed_configuration_and_native_counter_preserved(self):
  f=self.fixture('cancel');self.run_cli(f,130);args=f.argv(True);args[args.index('--speed-mkeys')+1]='999';before=(f.root/'coordinator-state.txt').read_bytes();self.run_cli(f,2,args=args);self.assertEqual((f.root/'coordinator-state.txt').read_bytes(),before);self.assertEqual(f.claims,1)
 def test_changed_native_checkpoint_refused_before_new_kernel(self):
  f=self.fixture('cancel');self.run_cli(f,130);cp=next(f.root.glob('segment-*/checkpoint.txt'));lines=cp.read_text().splitlines();row=lines[2].split();row[0]=str(int(row[0])+BATCH);lines[2]=' '.join(row);cp.write_text('\n'.join(lines)+'\n');before=len(f.kernels());f.kind='normal';self.run_cli(f,2,True);self.assertEqual(len(f.kernels()),before);self.assertEqual(f.completions,0)
 def test_owner_crash_resume_preserves_one_claim(self):
  f=self.fixture(count=20*BATCH+7);log=f.root/'owner.log'
  # Keep the outer test log outside the protected native root.
  log=self.directory/'owner.log'
  with log.open('w') as output:
   child=subprocess.Popen(f.argv(),env=f.env(),stdout=output,stderr=output,start_new_session=True)
   try:
    deadline=time.monotonic()+10
    while time.monotonic()<deadline and (not f.progress or f.progress[-1]==0):
     self.assertIsNone(child.poll());time.sleep(.01)
    self.assertTrue(f.progress and f.progress[-1]>0);child.kill();child.wait(timeout=5);time.sleep(.4)
   finally:
    if child.poll() is None:child.kill();child.wait()
  self.run_cli(f,0,True);self.assertEqual(f.claims,1);self.assertEqual(f.completions,1)
 def test_remote_endpoint_and_missing_optin_rejected(self):
  f=self.fixture();args=f.argv();args[args.index('--server')+1]='https://claudiodb.com';self.run_cli(f,2,args=args);self.assertFalse(f.requests);args=f.argv();args.remove('--enable-experimental-vanity-local');self.run_cli(f,2,args=args);self.assertFalse(f.requests)
 def test_solution_export_is_private_and_completion_not_sent(self):
  f=self.fixture(target=s.address(s.key_hash(s.BASE)));result=self.run_cli(f,10);self.assertEqual(f.completions,0);self.assertEqual(f.reports,1);wallets=list((f.root/'home').rglob('wallet-import.txt'));self.assertEqual(len(wallets),1);self.assertEqual(wallets[0].stat().st_mode&0o077,0);self.assertNotIn('private_key_wif=',result.stdout+result.stderr);self.run_cli(f,20,True);self.assertEqual(f.claims,1)
if __name__=='__main__':unittest.main(verbosity=2)
