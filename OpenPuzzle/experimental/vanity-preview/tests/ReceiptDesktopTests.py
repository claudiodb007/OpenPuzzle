"""Owned desktop receipt path through the existing control command contract."""
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import socket
import subprocess
import time
import unittest
from unittest.mock import patch
import CoordinatorDesktopTests as base
import ReceiptDesktop as d
import ReceiptEvidence as proof

c,s,e,p,CLI=base.c,base.s,base.e,base.p,base.base.CLI


class Tests(unittest.TestCase):
    def setUp(self):
        base.Tests.setUp(self)
        original=self.paths['engine'];engine=original.with_name('HeldCancellationEngine.py')
        source=original.read_text().replace("    print('OPV1 PROGRESS',checked,flush=True)",
            "    print('OPV1 PROGRESS',checked,flush=True)\n    if first==s.BASE and checked==2*batch and (Path(__file__).parent/'hold-for-cancel').exists():\n        deadline=time.monotonic()+10\n        while not stopped and time.monotonic()<deadline:time.sleep(.001)\n        assert stopped, 'Expected bounded fixture cancellation'")
        engine.write_text(source);engine.chmod(0o700);self.paths['engine']=engine
        self.proof['executables']['engine']=dict(path=str(engine),sha256=s.sha(engine))
        self.evidence=self.root/'receipt-measurement.json'
        s.write_json(self.evidence,self.proof);self.record=e.profile(self.proof,self.evidence,self.paths)
        s.write_json(self.candidate,self.record);p.import_profile(self.candidate,self.store)
        d.enable_store(self.store,CLI)
    tearDown=base.Tests.tearDown
    snapshot=base.Tests.snapshot

    @contextlib.contextmanager
    def hardware(self,marker=None):
        if marker=='cancel':(self.paths['engine'].parent/'hold-for-cancel').touch(mode=0o600)
        with base.Tests.hardware(self,marker):yield

    def start(self,marker=None):
        with self.hardware(marker):return d.execute(self.store,self.directory,CLI)
    def proposal(self):return d.snapshot(self.store,self.directory,CLI)[0]
    def resume(self):
        value=self.proposal()
        with self.hardware():return d.execute(self.store,self.directory,CLI,value['state_sha256'])
    def logs(self):return {str(p.relative_to(self.directory)):s.sha(p) for p in (self.directory/'native').glob('segment-*/attempt-*/engine.log')}
    def calls(self):return proof.read(self.directory/'http-requests.json')
    def operations(self,kind):return [r for r in self.calls() if r['path'].endswith('/operations') and r['payload']['kind']==kind]

    @contextlib.contextmanager
    def fault(self,kind):
        parent=d.peer.ReceiptAssignmentServer
        class Fault(parent):
            def respond(instance,handler,code,response):
                which=response.get('kind')
                if not getattr(instance,'used',False) and which=={'lost-claim':'claim','lost-progress':'progress','lost-complete':'complete'}.get(kind) and which is not None:
                    if which!='progress' or int(response['assignment']['keys_checked'])>0:
                        instance.used=True;handler.close_connection=True
                        try:handler.connection.shutdown(socket.SHUT_RDWR)
                        except OSError:pass
                        return
                return super().respond(handler,code,response)
            def handle(instance,path,body):
                if path.endswith('/operations'):
                    if kind=='unknown' and body['kind']=='claim':return 503,dict(error='temporary_unavailable')
                    if kind=='lease-before' and body['kind']=='progress':
                        instance.ledger.clock=lambda:int(time.time())+400
                return super().handle(path,body)
        with patch.object(d.peer,'ReceiptAssignmentServer',Fault):yield

    def test_two_segments_same_ui_command_contract_and_read_only_completed_listing(self):
        self.assertEqual(self.start(),0);state=self.proposal()
        self.assertEqual(state['native'],dict(phase='done',checked=c.TOTAL,total=c.TOTAL,launches=2,claims=1,completions=1))
        self.assertFalse(state['available']);self.assertEqual(len(self.operations('claim')),1);self.assertEqual(len(self.operations('complete')),1)
        before=self.snapshot()
        with contextlib.redirect_stdout(io.StringIO()) as output:c.list_runs(self.store,CLI)
        row=json.loads(output.getvalue())['runs'][0];self.assertFalse(row['available']);self.assertEqual(row['phase'],'done')
        self.assertEqual(before,self.snapshot());self.assertFalse((self.directory/c.ACTIVE).exists())

    def test_stop_close_reopen_resume_same_assignment_and_archive(self):
        self.assertEqual(self.start('cancel'),130);before=self.proposal();logs=self.logs()
        self.assertTrue(before['available']);self.assertTrue(0<before['native']['checked']<c.SEGMENT)
        self.assertEqual(self.resume(),0);after=self.proposal();self.assertEqual(after['native']['completions'],1)
        self.assertEqual(len(self.operations('claim')),1);self.assertEqual(len(self.operations('complete')),1)
        for name,digest in logs.items():self.assertEqual(s.sha(self.directory/name),digest)
        archive=next((self.directory/'coordinator-history').glob('*/snapshot.json')).parent
        for name,digest in before['files'].items():self.assertEqual(s.sha(archive/'before'/name),digest)

    def test_safe_stop_finishes_both_segments_without_next_claim(self):
        self.assertEqual(self.start('safe-stop'),0);self.assertEqual(self.proposal()['native']['launches'],2)
        self.assertEqual(len(self.operations('claim')),1);self.assertEqual(len(self.operations('complete')),1)

    def test_lost_claim_sealed_and_resumed_without_second_claim(self):
        with self.fault('lost-claim'):self.assertEqual(self.start(),2)
        state=self.proposal();self.assertEqual(state['native']['phase'],'claiming');self.assertTrue(state['available'])
        self.assertEqual(self.logs(),{});self.assertEqual(self.resume(),0)
        self.assertEqual(len(self.operations('claim')),1);self.assertEqual(self.proposal()['native']['completions'],1)

    def test_lost_progress_resume_preserves_every_prior_native_attempt(self):
        with self.fault('lost-progress'):self.assertEqual(self.start(),3)
        before=self.proposal();logs=self.logs();self.assertEqual(before['recovery']['kind'],'receipt-pending')
        self.assertEqual(self.resume(),0)
        for name,digest in logs.items():self.assertEqual(s.sha(self.directory/name),digest)
        self.assertEqual(len(self.operations('claim')),1)

    def test_lost_completion_adoption_queries_without_kernel_or_gpu_check(self):
        with self.fault('lost-complete'):self.assertEqual(self.start(),1)
        state=self.proposal();self.assertTrue(state['available']);self.assertEqual(state['native']['completions'],1)
        logs=self.logs();db=(self.directory/'peer/ledger.sqlite').read_bytes()
        with patch.object(p,'hardware',side_effect=AssertionError('GPU check forbidden')), \
             patch.object(s,'telemetry',side_effect=AssertionError('GPU telemetry forbidden')):
            self.assertEqual(d.execute(self.store,self.directory,CLI,state['state_sha256']),0)
        self.assertEqual(logs,self.logs());self.assertEqual((self.directory/'peer/ledger.sqlite').read_bytes(),db)
        self.assertEqual(len(self.operations('claim')),1);self.assertEqual(len(self.operations('complete')),1)
        self.assertEqual(sum(r['path'].endswith('/query') for r in self.calls()),1)

    def test_unknown_receipt_stays_for_review_without_new_claim(self):
        with self.fault('unknown'):
            with self.assertRaisesRegex(RuntimeError,'Receipt unknown'):self.start()
        before=self.snapshot();row=d.row(self.store,self.directory,CLI);self.assertFalse(row['available'])
        with self.assertRaisesRegex(RuntimeError,'requires review'):d.execute(self.store,self.directory,CLI,'0'*64)
        self.assertEqual(before,self.snapshot());self.assertEqual(self.logs(),{})

    def test_lease_refused_is_terminal_for_resume(self):
        with self.fault('lease-before'):self.assertEqual(self.start(),2)
        state=self.proposal();self.assertFalse(state['available']);self.assertEqual(state['native']['phase'],'rejected')
        before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'refused'):d.execute(self.store,self.directory,CLI,state['state_sha256'])
        self.assertEqual(before,self.snapshot());self.assertEqual(self.logs(),{})

    def test_stale_snapshot_and_changed_sealed_ledger_preserve_files(self):
        self.assertEqual(self.start('cancel'),130);state=self.proposal();before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'Stale receipt snapshot'):d.execute(self.store,self.directory,CLI,'0'*64)
        self.assertEqual(before,self.snapshot())
        path=self.directory/'peer/ledger.sqlite';data=path.read_bytes();path.write_bytes(data+b'changed');before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'sealed evidence changed'):d.execute(self.store,self.directory,CLI,state['state_sha256'])
        self.assertEqual(before,self.snapshot())

    def test_changed_checkpoint_and_missing_native_journal_block_resume(self):
        self.assertEqual(self.start('cancel'),130);state=self.proposal()
        path=self.directory/'native/receipt-client.json';path.rename(path.with_suffix('.preserved'))
        before=self.snapshot()
        with self.assertRaises(RuntimeError):d.execute(self.store,self.directory,CLI,state['state_sha256'])
        self.assertEqual(before,self.snapshot())

    def test_busy_port_fails_before_any_mutation(self):
        self.assertEqual(self.start('cancel'),130);state,meta,*_=d.snapshot(self.store,self.directory,CLI)
        with socket.socket() as sock:
            sock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);sock.bind(('127.0.0.1',int(meta['native_options']['--server'].rsplit(':',1)[1])));sock.listen()
            before=self.snapshot()
            with self.hardware(),self.assertRaises(OSError):d.execute(self.store,self.directory,CLI,state['state_sha256'])
            self.assertEqual(before,self.snapshot())

    def test_failed_spawn_preserves_prior_seal_live_and_native_evidence(self):
        self.assertEqual(self.start('cancel'),130);state=self.proposal();old=self.directory/c.SEAL;seal=s.sha(old)
        with self.hardware(),patch.object(d.subprocess,'Popen',side_effect=OSError('Synthetic spawn failure')):
            with self.assertRaises(OSError):d.execute(self.store,self.directory,CLI,state['state_sha256'])
        self.assertEqual(s.sha(old),seal);self.assertFalse((self.directory/c.ACTIVE).exists());self.assertTrue(self.proposal()['available'])
        self.assertEqual(self.resume(),0)

    def test_store_configuration_and_boolean_aliases_fail_closed(self):
        path=self.store/d.MODE;original=path.read_bytes()
        for v in (True,0,'false'):
            value=json.loads(original);value['production_enabled']=v;path.write_text(json.dumps(value))
            with self.assertRaisesRegex(RuntimeError,'configuration changed'):d.enabled(self.store,CLI)
            self.assertFalse(self.directory.exists())
        path.write_bytes(original);self.assertTrue(d.enabled(self.store,CLI))

    def test_stale_native_outcome_cannot_seal_failed_new_invocation(self):
        self.assertEqual(self.start('cancel'),130);state=self.proposal();seal=s.sha(self.directory/c.SEAL)
        class Failed:
            returncode=2
            pid=os.getpid()
            def poll(self):return 2
        with self.hardware(),patch.object(d.subprocess,'Popen',return_value=Failed()):
            with self.assertRaisesRegex(RuntimeError,'current native invocation'):
                d.execute(self.store,self.directory,CLI,state['state_sha256'])
        self.assertEqual(s.sha(self.directory/c.SEAL),seal);self.assertTrue((self.directory/c.ACTIVE).exists())

    def test_missing_store_mode_never_reinterprets_receipt_run(self):
        with self.fault('lost-claim'):self.assertEqual(self.start(),2)
        state=self.proposal();(self.store/d.MODE).unlink();before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'configuration missing'):
            d.execute(self.store,self.directory,CLI,state['state_sha256'])
        self.assertEqual(before,self.snapshot())

    def test_native_completion_still_needs_exact_terminal_proof(self):
        with self.fault('lost-complete'):self.assertEqual(self.start(),1)
        state=self.proposal();cp=self.directory/'native/segment-1/checkpoint.txt';cp.write_text(cp.read_text().replace('complete','interrupted'))
        before=self.snapshot()
        with self.assertRaises(RuntimeError):d.execute(self.store,self.directory,CLI,state['state_sha256'])
        self.assertEqual(before,self.snapshot())

    def test_public_run_session_listing_resume_uses_existing_ui_fields(self):
        with self.fault('lost-claim'):self.assertEqual(self.start(),2)
        tools=self.root/'tools';tools.mkdir();gpu=self.record['uuid'];inventory=tools/'nvidia-smi'
        inventory.write_text('#!/usr/bin/env python3\nimport sys\nq=sys.argv[1]\n'+
            'if q.startswith("--query-compute-apps"):pass\n'+
            'elif q=="--query-gpu=uuid,name,driver_version":print("'+gpu+', NVIDIA GeForce RTX 4070 SUPER, synthetic-driver")\n'+
            'elif q.startswith("--query-gpu=uuid,temperature.gpu"):print("'+gpu+',30,20,245,0,1000,1000,50")\n'+
            'else:raise SystemExit("Unexpected GPU query/write")\n');inventory.chmod(0o700)
        env=dict(os.environ,PATH=str(tools)+os.pathsep+os.environ['PATH'],PYTHONDONTWRITEBYTECODE='1')
        prefix=[str(CLI),'run','20','--engine','vanity','--backend','cuda','--vanity-session']
        before=self.snapshot();listed=subprocess.run(prefix+['runs','--store',str(self.store)],env=env,capture_output=True,text=True,timeout=15)
        self.assertEqual(listed.returncode,0,listed.stderr);row=json.loads(listed.stdout)['runs'][0]
        self.assertTrue(row['available']);self.assertEqual(row['checked'],0);self.assertEqual(before,self.snapshot())
        result=subprocess.run(prefix+['resume','--store',str(self.store),'--run-root',str(self.directory),
            '--expected-state',row['state_sha256'],'--enable-experimental-vanity-local'],env=env,capture_output=True,text=True,timeout=25)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertEqual(self.proposal()['native']['completions'],1)

    def test_public_start_selects_receipts_and_legacy_run_remains_separate(self):
        import uuid
        tools=self.root/'tools';tools.mkdir();gpu=self.record['uuid'];inventory=tools/'nvidia-smi'
        inventory.write_text('#!/usr/bin/env python3\nimport sys\nq=sys.argv[1]\n'+
            'if q.startswith("--query-compute-apps"):pass\n'+
            'elif q=="--query-gpu=uuid,name,driver_version":print("'+gpu+', NVIDIA GeForce RTX 4070 SUPER, synthetic-driver")\n'+
            'elif q.startswith("--query-gpu=uuid,temperature.gpu"):print("'+gpu+',30,20,245,0,1000,1000,50")\n'+
            'else:raise SystemExit("Unexpected GPU query/write")\n');inventory.chmod(0o700)
        env=dict(os.environ,PATH=str(tools)+os.pathsep+os.environ['PATH'],PYTHONDONTWRITEBYTECODE='1')
        prefix=[str(CLI),'run','20','--engine','vanity','--backend','cuda','--vanity-session']
        result=subprocess.run(prefix+['start','--store',str(self.store),'--run-root',str(self.directory),
            '--enable-experimental-vanity-local'],env=env,capture_output=True,text=True,timeout=25)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertEqual(proof.read(self.directory/c.META)['schema'],d.SCHEMA)
        legacy=self.store/('run-'+str(uuid.uuid4()))
        with self.hardware():self.assertEqual(c.execute(self.store,legacy,CLI),0)
        before={str(path.relative_to(legacy)):s.sha(path) for path in legacy.rglob('*') if path.is_file()}
        listed=subprocess.run(prefix+['runs','--store',str(self.store)],env=env,capture_output=True,text=True,timeout=15)
        self.assertEqual(listed.returncode,0,listed.stderr);self.assertEqual(len(json.loads(listed.stdout)['runs']),2)
        self.assertEqual(before,{str(path.relative_to(legacy)):s.sha(path) for path in legacy.rglob('*') if path.is_file()})


if __name__=='__main__':unittest.main(verbosity=2)
