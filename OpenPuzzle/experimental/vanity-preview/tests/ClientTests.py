#!/usr/bin/env python3
import os,shutil,signal,subprocess,sys,tempfile,time,unittest
from pathlib import Path
CLIENT=Path(sys.argv.pop(1)).resolve()
FAKE=Path(__file__).with_name('FakeEngine.py').resolve()
BASE='123456789000000000'
def state(root):
    words=(root/'checkpoint.txt').read_text().split()
    return {'binding':words[1],'start':words[2],'count':int(words[3]),'grid':int(words[4]),'mode':words[5],
            'checked':int(words[6]),'status':words[7],'safe':words[8]=='1'}
def wait_for(fn,p,seconds=8):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        if fn():return
        if p.poll() is not None:raise AssertionError('Process exited before fixture condition')
        time.sleep(.01)
    raise AssertionError('Fixture timeout')
class ClientTests(unittest.TestCase):
    def test_closed_progress_reader_stops_owned_engine_and_preserves_checkpoint(self):
        env=os.environ.copy();env['OPV_FAKE_MODE']='slow'
        p=subprocess.Popen(self.args(count=3145735),env=env,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True)
        self.children.append(p)
        self.assertTrue(p.stdout.readline().startswith('OPV_CLIENT PROGRESS '))
        p.stdout.close()
        self.assertEqual(p.wait(timeout=8),130)
        self.assertEqual(state(self.ws)['status'],'interrupted')
        self.assertLess(state(self.ws)['checked'],3145735)
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix="OpenPuzzle ç ; $() '")
        self.base=Path(self.temp.name);self.ws=self.base/'workspace';self.keys=self.base/'known keys.txt';self.keys.write_text(BASE+'\n')
        self.controls=self.base/'controls';self.controls.mkdir();self.children=[]
        self.fake=self.base/'synthetic engine';shutil.copyfile(FAKE,self.fake);self.fake.chmod(0o700)
    def tearDown(self):
        for p in self.children:
            if p.poll() is None:p.terminate();p.wait(timeout=5)
        self.temp.cleanup()
    def args(self,count=524291,mode='exhaust',binding='a'*64):
        return [str(CLIENT),str(self.fake),str(self.ws),BASE,str(count),'1',str(self.keys),mode,binding,str(self.controls),'--synthetic-only']
    def run_one(self,behavior='normal',**kw):
        env=os.environ.copy();env['OPV_FAKE_MODE']=behavior
        return subprocess.run(self.args(**kw),env=env,capture_output=True,text=True,timeout=10)
    def start(self,**kw):
        env=os.environ.copy();env['OPV_FAKE_MODE']='slow'
        log=(self.base/('client-'+str(len(self.children))+'.log')).open('w')
        p=subprocess.Popen(self.args(**kw),env=env,stdout=log,stderr=subprocess.STDOUT);log.close();self.children.append(p);return p
    def test_complete_tail_private_files_and_repeat_block(self):
        r=self.run_one();self.assertEqual(r.returncode,0,r.stderr);s=state(self.ws)
        self.assertEqual((s['checked'],s['status']),(524291,'complete'))
        self.assertEqual((self.ws/'checkpoint.txt').stat().st_mode&0o777,0o600)
        self.assertEqual(self.run_one().returncode,20)
        self.assertEqual(len(list(self.ws.glob('attempt-*'))),1)
    def test_missing_terminal_not_completion(self):
        self.assertNotEqual(self.run_one('missing').returncode,0)
        self.assertEqual(state(self.ws)['status'],'review');self.assertEqual(self.run_one().returncode,20)
    def test_nonzero_exit_not_completion(self):
        self.assertNotEqual(self.run_one('badexit').returncode,0);self.assertEqual(state(self.ws)['status'],'review')
    def test_forged_progress_faults_without_advancing(self):
        self.assertNotEqual(self.run_one('forged').returncode,0)
        self.assertEqual((state(self.ws)['checked'],state(self.ws)['status']),(0,'faulted'))
    def test_cancel_then_resume_suffix(self):
        n=5242880+7;p=self.start(count=n)
        wait_for(lambda:(self.ws/'checkpoint.txt').exists() and state(self.ws)['checked']>0,p)
        (self.controls/'cancel').write_text('cancel');p.wait(timeout=5);s=state(self.ws)
        self.assertEqual(s['status'],'interrupted');self.assertTrue(0<s['checked']<n)
        (self.controls/'cancel').rename(self.controls/'cancel-archived')
        r=self.run_one(count=n);self.assertEqual(r.returncode,0,r.stderr)
        self.assertIn('resumed='+str(s['checked']),r.stdout);self.assertEqual(state(self.ws)['checked'],n)
    def test_safe_stop_finishes_and_blocks_next(self):
        (self.controls/'safe-stop').write_text('safe');r=self.run_one()
        self.assertEqual(r.returncode,0,r.stderr);self.assertIn('next_blocked=1',r.stdout)
        self.assertTrue(state(self.ws)['safe']);self.assertEqual(state(self.ws)['status'],'complete')
    def test_solution_preserved_and_resume_blocked(self):
        r=self.run_one(mode='stop');self.assertEqual(r.returncode,10,r.stderr)
        files=list(self.ws.glob('attempt-*/solutions.txt'));self.assertEqual(len(files),1)
        before=files[0].read_bytes();self.assertNotIn('SYNTHETIC',r.stdout)
        self.assertEqual(self.run_one(mode='stop').returncode,20);self.assertEqual(files[0].read_bytes(),before)
    def test_binding_mismatch_preserved(self):
        self.run_one();before=(self.ws/'checkpoint.txt').read_bytes()
        self.assertEqual(self.run_one(binding='b'*64).returncode,2)
        self.assertEqual((self.ws/'checkpoint.txt').read_bytes(),before)
    def test_damaged_checkpoint_preserved(self):
        self.ws.mkdir();(self.ws/'checkpoint.txt').write_text('damaged')
        self.assertEqual(self.run_one().returncode,2);self.assertEqual((self.ws/'checkpoint.txt').read_text(),'damaged')
    def test_concurrent_owner_preserved(self):
        n=5242880+7;p=self.start(count=n)
        wait_for(lambda:(self.ws/'checkpoint.txt').exists() and state(self.ws)['checked']>0,p)
        r=self.run_one(count=n);self.assertEqual(r.returncode,2);self.assertIsNone(p.poll())
        (self.controls/'cancel').write_text('cancel');p.wait(timeout=5)
    def test_killed_owner_stops_child_and_recovers(self):
        n=5242880+7;p=self.start(count=n)
        wait_for(lambda:(self.ws/'checkpoint.txt').exists() and state(self.ws)['checked']>0,p)
        attempt=next(self.ws.glob('attempt-*/engine.pid')).parent;p.kill();p.wait(timeout=3)
        end=time.monotonic()+4
        dead=False
        while time.monotonic()<end:
            witness=attempt/'engine-stopped.txt'
            dead=witness.exists() and witness.read_text()=='SIGTERM\n'
            if dead:break
            time.sleep(.02)
        self.assertTrue(dead,'owned child did not observe parent-death termination')
        r=self.run_one(count=n);self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(state(self.ws)['checked'],n)
if __name__=='__main__':unittest.main(verbosity=2)
