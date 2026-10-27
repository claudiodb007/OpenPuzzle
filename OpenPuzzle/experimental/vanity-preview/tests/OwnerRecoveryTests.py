#!/usr/bin/env python3
"""Actual owned process loss and native resume; synthetic inventory/engine only."""
import contextlib
import io
import json
import math
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
import unittest
from unittest.mock import patch
import CoordinatorDesktopTests as desktop
import CoordinatorRecoveryTests as network

c,s,e,CLI=desktop.c,desktop.s,desktop.e,desktop.base.CLI


class Tests(unittest.TestCase):
    tearDown_base=desktop.Tests.tearDown
    hardware=desktop.Tests.hardware
    start=desktop.Tests.start
    snapshot=desktop.Tests.snapshot

    def setUp(self):
        desktop.Tests.setUp(self)
        original=self.paths['engine'];engine=original.with_name('OwnerTargetEngine.py')
        engine.write_text(original.read_text().replace('time.sleep(.003)','time.sleep(.01)'));engine.chmod(0o700)
        self.paths['engine']=engine
        self.proof['executables']['engine']=dict(path=str(engine),sha256=s.sha(engine))
        self.evidence=self.root/'owner-measurement.json';s.write_json(self.evidence,self.proof)
        self.record=e.profile(self.proof,self.evidence,self.paths);s.write_json(self.candidate,self.record)
        desktop.p.import_profile(self.candidate,self.store)
        tools=self.root/'tools';tools.mkdir();gpu=self.record['uuid']
        inventory=tools/'nvidia-smi'
        inventory.write_text('#!/usr/bin/env python3\nimport sys\nq=sys.argv[1]\n'+
            'if q.startswith("--query-compute-apps"):pass\n'+
            'elif q=="--query-gpu=uuid,name,driver_version":print("'+gpu+', NVIDIA GeForce RTX 4070 SUPER, synthetic-driver")\n'+
            'elif q.startswith("--query-gpu=uuid,temperature.gpu"):print("'+gpu+',30,20,245,0,1000,1000,50")\n'+
            'else:raise SystemExit("Unexpected GPU write/query")\n')
        inventory.chmod(0o700)
        self.env=dict(os.environ,PATH=str(tools)+os.pathsep+os.environ['PATH'],PYTHONDONTWRITEBYTECODE='1')
        self.prefix=[str(CLI),'run','20','--engine','vanity','--backend','cuda','--vanity-session']
        self.child=None

    def tearDown(self):
        if self.child is not None and self.child.poll() is None:
            self.child.terminate()
            try:self.child.wait(timeout=15)
            except subprocess.TimeoutExpired:self.child.kill();self.child.wait()
        # Only this test's explicitly recorded child identity may be stopped.
        path=self.directory/c.ACTIVE
        if path.is_file():
            identity=getattr(self,'native_identity',None)
            if identity and not c.owner.dead(identity):
                os.kill(identity['pid'],signal.SIGTERM)
                deadline=time.monotonic()+10
                while time.monotonic()<deadline and not c.owner.dead(identity):time.sleep(.02)
        self.tearDown_base()

    def spawn(self,mode=None):
        mode=mode or 'progress'
        command=self.prefix+['start','--store',str(self.store),'--run-root',str(self.directory),'--enable-experimental-vanity-local']
        self.marker=self.root/'controlled-stage'
        if mode:
            driver=self.root/'driver.py'
            driver.write_text('''import sys,time
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,sys.argv.pop(1))
import Coordinator as c
mode=sys.argv.pop(1);marker=Path(sys.argv.pop(1));parent=c.LocalAssignmentServer
class Controlled(parent):
 def handle(self,path,body):
  if mode=='claim' and path=='/api/range/claim':
   marker.touch()
   while True:time.sleep(.01)
  return super().handle(path,body)
 def respond(self,handler,code,response):
  if mode=='progress' and handler.path=='/api/range/progress' and self.progress and self.progress[-1]>0:
   marker.touch()
   while True:time.sleep(.01)
  if mode=='completion' and handler.path=='/api/range/complete':
   marker.touch()
   while True:time.sleep(.01)
  return super().respond(handler,code,response)
 def close(self):
  state=self.root/'coordinator-state.txt'
  if mode=='done' and state.is_file() and state.read_text().splitlines()[2].split()[0]=='done':
   marker.touch()
   while True:time.sleep(.01)
  return super().close()
c.LocalAssignmentServer=Controlled
raise SystemExit(c.main())
''')
            command=[sys.executable,str(driver),str(Path(c.__file__).parent),mode,str(self.marker),'start',
                     '--cli',str(CLI),'--store',str(self.store),'--run-root',str(self.directory),'--enable-experimental-vanity-local']
        self.log=self.root/'owned.log'
        with self.log.open('w') as log:self.child=subprocess.Popen(command,env=self.env,stdout=log,stderr=log,start_new_session=True)
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            self.assertIsNone(self.child.poll(),self.log.read_text())
            active=self.directory/c.ACTIVE
            live=self.directory/'http-live.json'
            if active.is_file() and live.is_file():
                a=e.read_json(active);value=e.read_json(live)
                ready=self.marker.exists() if mode else any(row['applied']['progress'] and int(row['payload']['keys_checked'])>0 for row in value['responses'])
                if a.get('native') and ready and a.get('journal_sha256')==s.sha(live):
                    self.native_identity=a['native'];return a
            time.sleep(.005)
        self.fail('Owned fixture did not reach its controlled stage: '+self.log.read_text())

    def lose(self,mode=None,native=False):
        active=self.spawn(mode)
        if native:
            os.kill(active['native']['pid'],signal.SIGKILL)
            self.child.wait(timeout=15)
        else:
            self.child.kill();self.child.wait(timeout=5)
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            try:
                c.native_idle(self.directory)
                for lock_path in (self.directory/'native').glob('segment-*/owner.lock'):
                    import fcntl
                    with lock_path.open('rb') as lock:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
                if c.owner.dead(active['native']):return active
            except BlockingIOError:pass
            time.sleep(.02)
        self.fail('Native fixture did not stop after owned process loss')

    def proposal(self):return c.owner_proposal(self.store,self.directory,CLI)[0]

    def resume_proposal(self,proposal):
        with self.hardware():return c.execute(self.store,self.directory,CLI,proposal['state_sha256'])

    def test_paced_progress_keeps_every_engine_batch_and_mandatory_flush(self):
        start=time.monotonic();self.assertEqual(self.start(),0);elapsed=time.monotonic()-start
        calls=e.read_json(self.directory/'http-requests.json')
        progress=[int(row['payload']['keys_checked']) for row in calls if row['path'].endswith('/progress')]
        batch=self.record['grid']*256*1024
        self.assertTrue({0,batch,c.SEGMENT,c.TOTAL}<=set(progress))
        self.assertLessEqual(len(progress),8+math.ceil(elapsed/5))
        engine_batches=sum(line.startswith('OPV1 PROGRESS ') for path in (self.directory/'native').glob('segment-*/attempt-*/engine.log') for line in path.read_text().splitlines())
        self.assertEqual(engine_batches,c.TOTAL//batch)
        self.assertLess(len(progress),engine_batches//4)
        live=e.read_json(self.directory/'http-live.json')
        self.assertIsNone(live['pending']);self.assertEqual(live['requests'],calls)
        self.assertEqual(live['responses'],e.read_json(self.directory/'http-responses.json'))

    def test_helper_sigkill_public_resume_preserves_assignment(self):
        self.lose()
        listed=subprocess.run(self.prefix+['runs','--store',str(self.store)],env=self.env,capture_output=True,text=True,timeout=15)
        self.assertEqual(listed.returncode,0,listed.stderr)
        row=json.loads(listed.stdout)['runs'][0]
        self.assertTrue(row['available'],row);self.assertEqual(row['recovery'],'owner-loss')
        result=subprocess.run(self.prefix+['resume','--store',str(self.store),'--run-root',str(self.directory),
            '--expected-state',row['state_sha256'],'--enable-experimental-vanity-local'],env=self.env,capture_output=True,text=True,timeout=20)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        seal=c.inspect(self.store,self.directory,CLI,allow_complete=True)[0]
        self.assertEqual((seal['native']['checked'],seal['native']['claims'],seal['native']['completions']),(c.TOTAL,1,1))
        self.assertTrue(list((self.directory/'coordinator-history').glob('*/owner-proposal.json')))

    def test_native_sigkill_reconciles_durable_checkpoint_before_probe(self):
        self.lose(native=True)
        before=self.proposal()
        logs={str(p.relative_to(self.directory)):s.sha(p) for p in (self.directory/'native').glob('segment-*/attempt-*/engine.log')}
        self.assertEqual(self.resume_proposal(before),0)
        for name,digest in logs.items():self.assertEqual(s.sha(self.directory/name),digest)
        self.assertEqual(c.inspect(self.store,self.directory,CLI,allow_complete=True)[0]['native']['completions'],1)

    def test_live_owner_cannot_be_recovered(self):
        self.spawn()
        with self.assertRaisesRegex(RuntimeError,'still active'):
            self.proposal()
        self.assertIsNone(self.child.poll())
        self.assertFalse((self.directory/'coordinator-history').exists())

    def test_pending_claim_intent_is_not_replayed(self):
        self.lose('claim');before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'ambiguous live HTTP'):
            self.proposal()
        self.assertEqual(before,self.snapshot())
        self.assertFalse(list((self.directory/'native').glob('segment-*/attempt-*')))

    def test_applied_completion_without_reply_stays_for_review(self):
        self.lose('completion');before=self.snapshot()
        with self.assertRaises(RuntimeError):self.proposal()
        self.assertEqual(before,self.snapshot())
        live=e.read_json(self.directory/'http-live.json')
        self.assertEqual(sum(row['applied']['complete'] for row in live['responses']),1)

    def test_confirmed_done_adoption_has_no_process_or_rpc(self):
        self.lose('done');proposal=self.proposal()
        self.assertEqual(proposal['native']['phase'],'done')
        live=(self.directory/'http-live.json').read_bytes()
        with patch.object(c.subprocess,'Popen',side_effect=AssertionError('Unexpected process launch')),patch.object(c.p,'hardware',side_effect=AssertionError('Unexpected GPU check')):
            self.assertEqual(c.execute(self.store,self.directory,CLI,proposal['state_sha256']),0)
        self.assertEqual((self.directory/'http-live.json').read_bytes(),live)
        self.assertFalse((self.directory/c.ACTIVE).exists())
        self.assertEqual(c.inspect(self.store,self.directory,CLI,allow_complete=True)[0]['native']['completions'],1)

    def test_changed_live_journal_is_blocked_without_mutation(self):
        self.lose();path=self.directory/'http-live.json'
        value=e.read_json(path);value['responses'][-1]['status']=400;s.write_json(path,value)
        before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'Live HTTP journal changed'):self.proposal()
        self.assertEqual(before,self.snapshot())

    def test_stale_owner_confirmation_preserves_files(self):
        self.lose();proposal=self.proposal()
        note=self.directory/'owner-note.txt';note.write_text('extra review evidence');note.chmod(0o600)
        before=self.snapshot()
        with self.hardware(),self.assertRaisesRegex(RuntimeError,'Stale owner recovery confirmation'):
            c.execute(self.store,self.directory,CLI,proposal['state_sha256'])
        self.assertEqual(before,self.snapshot())

    def test_unknown_process_identity_stays_for_review(self):
        self.lose();path=self.directory/c.ACTIVE;active=e.read_json(path);active['native']['start_ticks']='unknown';s.write_json(path,active)
        before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'Unknown process identity'):self.proposal()
        self.assertEqual(before,self.snapshot())

    def test_cold_recovery_lease_rejection_launches_no_kernel(self):
        self.lose();proposal=self.proposal()
        logs={str(p.relative_to(self.directory)):s.sha(p) for p in (self.directory/'native').glob('segment-*/attempt-*/engine.log')}
        with network.Tests.fault(self,'lease-before'):
            self.assertEqual(self.resume_proposal(proposal),2)
        for name,digest in logs.items():self.assertEqual(s.sha(self.directory/name),digest)
        seal=c.inspect(self.store,self.directory,CLI,allow_complete=True)[0]
        self.assertEqual((seal['native']['phase'],seal['native']['claims'],seal['native']['completions']),('rejected',1,0))


if __name__=='__main__':unittest.main(verbosity=2)
