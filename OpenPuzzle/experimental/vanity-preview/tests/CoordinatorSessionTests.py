#!/usr/bin/env python3
"""Native HttpRangeClient, native adapter, loopback server and synthetic GPU engine."""
import os,signal,subprocess,sys,tempfile,time,unittest
from pathlib import Path
CLI=Path(sys.argv.pop(1)).resolve();ADAPTER=Path(sys.argv.pop(1)).resolve()
sys.path.insert(0,str(Path(__file__).parent.parent/'measured'))
from LocalCoordinator import LocalAssignmentServer
import Support as s
UUID='GPU-11111111-2222-3333-4444-555555555555'
ENGINE=Path(__file__).with_name('FakeTargetEngine.py').resolve()
BATCH=256*1024

class Fixture(LocalAssignmentServer):
 """Test-only defaults and faults around the shared local runtime contract."""
 def __init__(self,root,kind='normal',engine=ENGINE,count=5*BATCH+7,grid=1,segment=2*BATCH,target=None,planning=2519.41,*,gpu_uuid=None,port=0,client_id=None,resume_only=False,initial_checked=0):
  self.kind=kind
  super().__init__(root,cli=CLI,adapter=ADAPTER,engine=engine,count=count,grid=grid,segment=segment,
   target=target or s.address(s.key_hash(s.BASE-1)),planning=planning,gpu_uuid=gpu_uuid or UUID,
   port=port,client_id=client_id,resume_only=resume_only,initial_checked=initial_checked)
 def handle(self,path,b):
  if path=='/api/range/claim':
   code,response=super().handle(path,b)
   if self.kind=='unavailable':return 200,dict(available=False,reason='No work')
   if self.kind=='ambiguous':return 503,dict(error='temporary_unavailable')
   if self.kind=='redirect':return 302,dict(error='redirect')
   if self.kind=='wrong-target':response['target']=s.address(s.key_hash(s.BASE))
   if self.kind=='outside-scope':response['end']=f'{s.BASE+self.count:064x}'
   return code,response
  if path=='/api/range/progress':
   self.assignment_identity(b);n=int(b['keys_checked'])
   if self.kind=='bad-ack':return 200,dict(success=False)
   if self.kind=='lease-before' or self.kind=='lease-during' and n>0:return 409,dict(error='assignment_lease_expired')
   if self.kind=='progress-fail' and n>0:return 503,dict(error='temporary_unavailable')
   if self.kind=='progress-once' and n>0 and not getattr(self,'failed_once',False):self.failed_once=True;return 503,dict(error='temporary_unavailable')
   reply=super().handle(path,b)
   if self.kind in ('cancel','safe') and n>0:
    (self.root/'controls'/('cancel' if self.kind=='cancel' else 'safe-stop')).touch(mode=0o600)
   return reply
  if path=='/api/range/complete':
   if self.kind=='completion-fail' or self.kind=='completion-once' and self.complete_attempts==0:
    self.assignment_identity(b);assert int(b['keys_checked'])==self.count and self.progress[-1]==self.count and b['status']=='completed' and b['exit_code']==0
    self.complete_attempts+=1;return 503,dict(error='temporary_unavailable')
  return super().handle(path,b)
 def argv(self,resume=False):
  args=[str(CLI),'vanity-preview','coordinator-test','--enable-experimental-vanity-local']
  for k,v in self.options().items():args+=[k,v]
  if resume:args+=['--resume']
  return args
 def env(self):return dict(os.environ,CUDA_VISIBLE_DEVICES=self.gpu_uuid,HOME=str(self.root/'home'),XDG_CONFIG_HOME=str(self.root/'home/.config'),XDG_DATA_HOME=str(self.root/'home/.local/share'),OPENPUZZLE_EXECUTION_SLOT='cuda-0')
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
