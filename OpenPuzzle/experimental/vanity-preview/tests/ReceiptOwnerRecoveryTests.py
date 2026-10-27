"""Actual owned helper/native loss, durable peer and explicit snapshot resume."""
import contextlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import time
import unittest
from unittest.mock import patch
import OwnerRecoveryTests as old
import ReceiptDesktop as d
import ReceiptEvidence as proof

c,s,e,CLI=old.c,old.s,old.e,old.CLI


class Tests(unittest.TestCase):
    def setUp(self):
        old.Tests.setUp(self);d.enable_store(self.store,CLI)
    tearDown=old.Tests.tearDown
    tearDown_base=old.Tests.tearDown_base
    hardware=old.Tests.hardware
    snapshot=old.Tests.snapshot
    lose=old.Tests.lose

    def proposal(self):return d.snapshot(self.store,self.directory,CLI)[0]
    def resume(self,state):
        with self.hardware():return d.execute(self.store,self.directory,CLI,state['state_sha256'])
    def logs(self):return {str(p.relative_to(self.directory)):s.sha(p) for p in (self.directory/'native').glob('segment-*/attempt-*/engine.log')}

    def spawn(self,mode=None):
        mode=mode or 'progress';self.marker=self.root/'controlled-receipt-stage';driver=self.root/'receipt-driver.py'
        driver.write_text('''import sys,time
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,sys.argv.pop(1))
import ReceiptDesktop as d
import Coordinator as c
mode=sys.argv.pop(1);marker=Path(sys.argv.pop(1));parent=d.peer.ReceiptAssignmentServer
class Controlled(parent):
 def handle(self,path,body):
  result=super().handle(path,body)
  if path.endswith('/operations') and body['kind']=={'claim':'claim','completion':'complete'}.get(mode):
   marker.touch()
   while True:time.sleep(.01)
  return result
 def respond(self,handler,code,response):
  if mode=='progress' and response.get('kind')=='progress' and int(response['assignment']['keys_checked'])>0:
   marker.touch()
   while True:time.sleep(.01)
  return super().respond(handler,code,response)
 def close(self):
  state=self.root/'coordinator-state.txt'
  if mode=='done' and state.is_file() and state.read_text().splitlines()[2].startswith('done '):
   marker.touch()
   while True:time.sleep(.01)
  return super().close()
d.peer.ReceiptAssignmentServer=Controlled
sys.argv=['receipt-owner',*sys.argv[1:]]
raise SystemExit(c.main())
''')
        self.log=self.root/'receipt-owner.log'
        args=[sys.executable,'-B',str(driver),str(Path(c.__file__).parent),mode,str(self.marker),'start','--cli',str(CLI),
              '--store',str(self.store),'--run-root',str(self.directory),'--enable-experimental-vanity-local']
        with self.log.open('w') as output:self.child=subprocess.Popen(args,env=self.env,stdout=output,stderr=output)
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            if self.child.poll() is not None:self.fail('Receipt owner ended before boundary: '+self.log.read_text())
            active=self.directory/c.ACTIVE;live=self.directory/'http-live.json'
            if self.marker.exists() and active.is_file() and live.is_file():
                a=proof.read(active)
                if a.get('native') and a.get('journal_sha256')==s.sha(live):
                    self.native_identity=a['native'];return a
            time.sleep(.005)
        self.fail('Receipt owner boundary not reached: '+self.log.read_text())

    def test_helper_loss_progress_public_list_and_resume(self):
        self.lose();before=self.snapshot()
        listed=subprocess.run(self.prefix+['runs','--store',str(self.store)],env=self.env,capture_output=True,text=True,timeout=15)
        self.assertEqual(listed.returncode,0,listed.stderr);row=json.loads(listed.stdout)['runs'][0]
        self.assertTrue(row['available'],row);self.assertEqual(row['recovery'],'receipt-owner-loss');self.assertEqual(before,self.snapshot())
        result=subprocess.run(self.prefix+['resume','--store',str(self.store),'--run-root',str(self.directory),
            '--expected-state',row['state_sha256'],'--enable-experimental-vanity-local'],env=self.env,capture_output=True,text=True,timeout=25)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        state=self.proposal();self.assertEqual((state['native']['claims'],state['native']['completions']),(1,1))

    def test_applied_claim_with_pending_http_recovers_without_redelivery(self):
        self.lose('claim');state=self.proposal();self.assertEqual(state['native']['phase'],'claiming');self.assertEqual(self.logs(),{})
        self.assertEqual(self.resume(state),0)
        calls=proof.read(self.directory/'http-requests.json')
        self.assertEqual(sum(r['path'].endswith('/operations') and r['payload']['kind']=='claim' for r in calls),1)
        responses=proof.read(self.directory/'http-responses.json')
        self.assertEqual(sum(r['status']==599 and r['reply'].get('error')=='receipt_resolved_without_delivery' for r in responses),1)
        self.assertEqual(self.proposal()['native']['completions'],1)

    def test_applied_completion_with_pending_http_recovers_without_gpu_or_kernel(self):
        self.lose('completion');state=self.proposal();self.assertEqual(state['native']['checked'],c.TOTAL)
        logs=self.logs();db=(self.directory/'peer/ledger.sqlite').read_bytes()
        with patch.object(d.p,'hardware',side_effect=AssertionError('Unexpected GPU check')), \
             patch.object(s,'telemetry',side_effect=AssertionError('Unexpected GPU telemetry')):
            self.assertEqual(d.execute(self.store,self.directory,CLI,state['state_sha256']),0)
        self.assertEqual(logs,self.logs());self.assertEqual((self.directory/'peer/ledger.sqlite').read_bytes(),db)
        self.assertEqual(self.proposal()['native']['completions'],1)

    def test_native_sigkill_reconciles_only_verified_prefix(self):
        self.lose(native=True);state=self.proposal();logs=self.logs();self.assertEqual(self.resume(state),0)
        for name,digest in logs.items():self.assertEqual(s.sha(self.directory/name),digest)
        self.assertEqual(self.proposal()['native']['completions'],1)

    def test_done_adoption_needs_no_process_socket_or_gpu(self):
        self.lose('done');state=self.proposal();self.assertEqual(state['native']['phase'],'done')
        live=(self.directory/'http-live.json').read_bytes();logs=self.logs()
        with patch.object(d.subprocess,'Popen',side_effect=AssertionError('Unexpected process')), \
             patch.object(d.p,'hardware',side_effect=AssertionError('Unexpected GPU check')), \
             patch.object(socket,'socket',side_effect=AssertionError('Unexpected socket')):
            self.assertEqual(d.execute(self.store,self.directory,CLI,state['state_sha256']),0)
        self.assertEqual(live,(self.directory/'http-live.json').read_bytes());self.assertEqual(logs,self.logs())
        self.assertFalse((self.directory/c.ACTIVE).exists());self.assertEqual(self.proposal()['native']['completions'],1)

    def test_live_or_unknown_owner_cannot_be_recovered(self):
        self.spawn()
        with self.assertRaisesRegex(RuntimeError,'still active'):self.proposal()
        self.assertIsNone(self.child.poll())

    def test_unknown_identity_after_loss_preserves_evidence(self):
        self.lose();path=self.directory/c.ACTIVE;active=proof.read(path);active['native']['start_ticks']='unknown';s.write_json(path,active)
        before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'Unknown process identity'):self.proposal()
        self.assertEqual(before,self.snapshot())

    def test_stale_snapshot_and_changed_live_head_are_blocked_without_writes(self):
        self.lose();state=self.proposal();note=self.directory/'review-note.txt';note.write_text('preserve');note.chmod(0o600)
        before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'Stale receipt snapshot'):d.execute(self.store,self.directory,CLI,state['state_sha256'])
        self.assertEqual(before,self.snapshot())
        path=self.directory/'http-live.json';v=proof.read(path);v['responses'][-1]['status']=400;s.write_json(path,v);before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'Live receipt HTTP journal changed'):self.proposal()
        self.assertEqual(before,self.snapshot())

    def test_changed_native_intent_digest_requires_review(self):
        self.lose('claim');path=self.directory/'native/receipt-client.json';v=proof.read(path);v['entries']['0']['request_sha256']='0'*64;s.write_json(path,v)
        before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'digest'):self.proposal()
        self.assertEqual(before,self.snapshot())

    def test_missing_ledger_never_creates_another_database(self):
        self.lose('claim');path=self.directory/'peer/ledger.sqlite';path.rename(path.with_suffix('.preserved'));before=self.snapshot()
        with self.assertRaises(RuntimeError):self.proposal()
        self.assertFalse(path.exists());self.assertEqual(before,self.snapshot())

    def test_ledger_sidecar_is_not_repaired_during_listing(self):
        self.lose('claim');path=self.directory/'peer/ledger.sqlite-journal'
        path.write_bytes(b'synthetic interrupted transaction');path.chmod(0o600);before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'no automatic repair'):self.proposal()
        self.assertEqual(before,self.snapshot())

    def test_extra_peer_receipt_without_native_intent_is_blocked(self):
        import uuid
        self.lose('claim');meta=proof.read(self.directory/c.META)
        ledger=proof.ledger(self.directory,meta)
        q=dict(proof.read(self.directory/'native/receipt-client.json')['entries']['0']['request'])
        q['operation_id']=str(uuid.uuid4());ledger.execute(q);before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'extra operations'):self.proposal()
        self.assertEqual(before,self.snapshot())


if __name__=='__main__':unittest.main(verbosity=2)
