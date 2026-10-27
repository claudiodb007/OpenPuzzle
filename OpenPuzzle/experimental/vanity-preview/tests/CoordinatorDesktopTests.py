#!/usr/bin/env python3
"""Actual native RunSession/HTTP/adapter; synthetic GPU inventory and engine only."""
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import socket
import threading
import time
import unittest
import uuid
from unittest.mock import patch
import MeasuredProfileTests as base
import Profile as p
import Coordinator as c
import Evidence as e
import Support as s

class Tests(unittest.TestCase):
    def setUp(self):
        base.Tests.setUp(self)
        engine_dir=self.root/'engine';engine_dir.mkdir()
        shutil.copytree(self.engine.parent/'support',engine_dir/'support')
        engine=engine_dir/'FakeTargetEngine.py'
        engine.write_text(self.engine.read_text().replace('time.sleep(.12)','time.sleep(.003)'));engine.chmod(0o700)
        self.paths['engine']=engine;self.proof['uuid']='GPU-aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee'
        self.proof['executables']['engine']=dict(path=str(engine),sha256=s.sha(engine))
        s.write_json(self.evidence,self.proof);self.record=e.profile(self.proof,self.evidence,self.paths);s.write_json(self.candidate,self.record)
        p.import_profile(self.candidate,self.store)
        self.directory=self.store/('run-'+str(uuid.uuid4()))

    tearDown=base.Tests.tearDown

    @contextlib.contextmanager
    def hardware(self,marker=None):
        stop=threading.Event();worker=None
        if marker:
            def trigger():
                deadline=time.monotonic()+15
                while time.monotonic()<deadline and not stop.is_set():
                    cp=next((self.directory/'native').glob('segment-*/checkpoint.txt'),None)
                    if cp and int(cp.read_text().split()[6])>=2*1024*256*1024:
                        (self.directory/'native/controls'/marker).touch(mode=0o600);return
                    time.sleep(.001)
            worker=threading.Thread(target=trigger,daemon=True);worker.start()
        with patch.object(p,'hardware',return_value=(self.record['uuid'],245)),patch.object(s,'telemetry',return_value=dict(temperature_c=30,power_limit_w=245)),patch.object(s,'compute_pids',return_value=[]),contextlib.redirect_stdout(io.StringIO()):
            try:yield
            finally:
                stop.set()
                if worker:worker.join(timeout=2)

    def start(self,marker=None):
        with self.hardware(marker):return c.execute(self.store,self.directory,base.CLI)

    def resume(self,marker=None):
        with self.hardware(marker):return c.execute(self.store,self.directory,base.CLI,s.sha(self.directory/c.SEAL))

    def snapshot(self):return {str(f.relative_to(self.directory)):s.sha(f) for f in self.directory.rglob('*') if f.is_file()}

    def test_run_session_two_segments_and_measured_uuid(self):
        self.assertEqual(self.start(),0)
        seal,meta,record,paths=c.inspect(self.store,self.directory,base.CLI,allow_complete=True)
        self.assertEqual(seal['native'],dict(phase='done',checked=c.TOTAL,total=c.TOTAL,launches=2,claims=1,completions=1))
        self.assertEqual(meta['native_options']['--gpu-uuid'],self.record['uuid'])
        self.assertEqual(meta['argv'][1:7],['run','20','--engine','vanity','--backend','cuda'])
        self.assertIn('--vanity-native-local',meta['argv'])
        with self.assertRaises(RuntimeError):self.resume()

    def test_stop_reopen_resume_same_assignment_exact_coverage(self):
        self.assertEqual(self.start('cancel'),130)
        seal,meta,record,paths=c.inspect(self.store,self.directory,base.CLI)
        self.assertTrue(0<seal['native']['checked']<c.SEGMENT)
        calls=e.read_json(self.directory/'http-requests.json');self.assertEqual(sum(r['path'].endswith('/claim') for r in calls),1)
        self.assertEqual(self.resume(),0)
        after=c.inspect(self.store,self.directory,base.CLI,allow_complete=True)[0]
        self.assertEqual(after['native']['checked'],c.TOTAL);self.assertEqual(after['native']['launches'],3)
        calls=e.read_json(self.directory/'http-requests.json');self.assertEqual(sum(r['path'].endswith('/claim') for r in calls),1);self.assertEqual(sum(r['path'].endswith('/complete') for r in calls),1)
        history=next((self.directory/'coordinator-history').iterdir())
        for name,digest in seal['files'].items():self.assertEqual(s.sha(history/'before'/name),digest)

    def stop_at_progress(self,final,wait_terminal=False):
        parent=c.LocalAssignmentServer
        def server(*args,**kwargs):
            class Trigger(parent):
                def handle(instance,path,body):
                    reply=super().handle(path,body)
                    if path=='/api/range/progress' and int(body['keys_checked'])==(c.TOTAL if final else 0):
                        if wait_terminal:
                            # The adapter runs independently of this HTTP callback.
                            # Request Stop only after its exact COMPLETE proof is durable.
                            checkpoint=instance.root/'segment-1/checkpoint.txt';deadline=time.monotonic()+5
                            while time.monotonic()<deadline and (not checkpoint.is_file() or checkpoint.read_text().split()[7]!='complete'):
                                time.sleep(.001)
                            assert checkpoint.is_file() and checkpoint.read_text().split()[7]=='complete', 'Terminal checkpoint was not confirmed'
                        (instance.root/'controls/cancel').touch(mode=0o600)
                    return reply
            return Trigger(*args,**kwargs)
        with self.hardware(),patch.object(c,'LocalAssignmentServer',side_effect=server):
            self.assertEqual(c.execute(self.store,self.directory,base.CLI),130)

    def test_stop_before_first_kernel_resumes_assigned_range(self):
        self.stop_at_progress(False)
        before=c.inspect(self.store,self.directory,base.CLI)[0]['native']
        self.assertEqual((before['checked'],before['launches']),(0,0))
        self.assertEqual(self.resume(),0)
        after=c.inspect(self.store,self.directory,base.CLI,allow_complete=True)[0]['native']
        self.assertEqual((after['claims'],after['launches'],after['completions']),(1,2,1))

    def test_stop_at_final_batch_without_terminal_stays_for_review(self):
        # A final PROGRESS is not a COMPLETE record. Force that ordering, so an
        # optimized adapter cannot accidentally turn this into the other race.
        engine=self.paths['engine'].with_name('DelayedTerminalEngine.py')
        source=self.paths['engine'].read_text()
        source=source.replace("    print('OPV1 PROGRESS',checked,flush=True)",
            "    print('OPV1 PROGRESS',checked,flush=True)\n    if checked==count and first==s.BASE+2**35:\n        deadline=time.monotonic()+5\n        while not stopped and time.monotonic()<deadline:time.sleep(.001)\n        assert stopped, 'Expected adapter cancellation before terminal'")
        engine.write_text(source);engine.chmod(0o700);self.paths['engine']=engine
        self.proof['executables']['engine']=dict(path=str(engine),sha256=s.sha(engine))
        self.evidence=self.root/'delayed-terminal-measurement.json'
        s.write_json(self.evidence,self.proof);self.record=e.profile(self.proof,self.evidence,self.paths)
        s.write_json(self.candidate,self.record);p.import_profile(self.candidate,self.store)
        with self.assertRaisesRegex(RuntimeError,'Native checkpoint requires review'):
            self.stop_at_progress(True)
        checkpoint=self.directory/'native/segment-1/checkpoint.txt'
        self.assertEqual(checkpoint.read_text().split()[6:8],[str(c.SEGMENT),'review'])
        self.assertTrue((self.directory/c.ACTIVE).is_file())
        calls=e.read_json(self.directory/'http-requests.json')
        self.assertEqual(sum(row['path'].endswith('/claim') for row in calls),1)
        self.assertEqual(sum(row['path'].endswith('/complete') for row in calls),0)
        before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'Coordinator transaction requires review'):
            c.execute(self.store,self.directory,base.CLI,'0'*64)
        self.assertEqual(before,self.snapshot())

    def test_stop_after_verified_terminal_resumes_completion_without_kernel(self):
        self.stop_at_progress(True,wait_terminal=True)
        before=c.inspect(self.store,self.directory,base.CLI)[0]['native']
        self.assertEqual((before['checked'],before['launches'],before['completions']),(c.TOTAL,2,0))
        self.assertEqual(self.resume(),0)
        after=c.inspect(self.store,self.directory,base.CLI,allow_complete=True)[0]['native']
        self.assertEqual((after['claims'],after['launches'],after['completions']),(1,2,1))

    def test_safe_stop_finishes_both_segments(self):
        self.assertEqual(self.start('safe-stop'),0)
        self.assertEqual(c.inspect(self.store,self.directory,base.CLI,allow_complete=True)[0]['native']['launches'],2)

    def test_stale_confirmation_and_changed_checkpoint_preserve_files(self):
        self.assertEqual(self.start('cancel'),130);before=self.snapshot()
        with self.hardware():
            with self.assertRaises(RuntimeError):c.execute(self.store,self.directory,base.CLI,'0'*64)
        self.assertEqual(before,self.snapshot())
        cp=next((self.directory/'native').glob('segment-*/checkpoint.txt'));cp.write_text(cp.read_text()+'damaged\n');before=self.snapshot()
        with self.assertRaises(RuntimeError):self.resume()
        self.assertEqual(before,self.snapshot())

    def test_busy_port_refused_before_mutation(self):
        self.assertEqual(self.start('cancel'),130);meta=e.read_json(self.directory/c.META);port=int(meta['native_options']['--server'].rsplit(':',1)[1])
        with socket.socket() as sock:
            sock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);sock.bind(('127.0.0.1',port));sock.listen();before=self.snapshot()
            with self.assertRaises(OSError):self.resume()
            self.assertEqual(before,self.snapshot())
        self.assertEqual(self.resume(),0)

    def test_failed_spawn_preserves_seal_and_cancel_can_retry(self):
        self.assertEqual(self.start('cancel'),130);seal=c.inspect(self.store,self.directory,base.CLI)[0]
        with self.hardware(),patch.object(c.subprocess,'Popen',side_effect=OSError('failed fixture spawn')):
            with self.assertRaises(OSError):c.execute(self.store,self.directory,base.CLI,s.sha(self.directory/c.SEAL))
        self.assertFalse((self.directory/c.ACTIVE).exists());self.assertEqual(c.inspect(self.store,self.directory,base.CLI)[0],seal)
        self.assertEqual(self.resume(),0)

    def test_active_transaction_and_busy_native_owner_refused(self):
        self.assertEqual(self.start('cancel'),130);before=self.snapshot()
        import fcntl
        with (self.directory/'native/owner.lock').open('rb') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            with self.assertRaises(BlockingIOError):self.resume()
        self.assertEqual(before,self.snapshot())
        (self.directory/c.ACTIVE).write_text('{}');before=self.snapshot()
        with self.assertRaises(RuntimeError):self.resume()
        self.assertEqual(before,self.snapshot())

    def test_owned_run_session_signal_cancel_list_and_resume(self):
        import subprocess
        tools=self.root/'tools';tools.mkdir();inventory=tools/'nvidia-smi'
        gpu=self.record['uuid']
        inventory.write_text('#!/usr/bin/env python3\nimport sys\nquery=sys.argv[1]\n'+
            'if query.startswith("--query-compute-apps"):pass\n'+
            'elif query=="--query-gpu=uuid,name,driver_version":print("'+gpu+', NVIDIA GeForce RTX 4070 SUPER, synthetic-driver")\n'+
            'elif query.startswith("--query-gpu=uuid,temperature.gpu"):print("'+gpu+',30,20,245,0,1000,1000,50")\n'+
            'else:raise SystemExit("Unexpected GPU write/query")\n')
        inventory.chmod(0o700);env=dict(os.environ,PATH=str(tools)+os.pathsep+os.environ['PATH'])
        prefix=[str(base.CLI),'run','20','--engine','vanity','--backend','cuda','--vanity-session']
        with (self.root/'owned-route.log').open('w') as log:
            child=subprocess.Popen(prefix+['start','--store',str(self.store),'--run-root',str(self.directory),'--enable-experimental-vanity-local'],env=env,stdout=log,stderr=log,start_new_session=True)
            try:
                deadline=time.monotonic()+15
                while time.monotonic()<deadline:
                    cp=next((self.directory/'native').glob('segment-*/checkpoint.txt'),None)
                    if cp and int(cp.read_text().split()[6])>0:break
                    self.assertIsNone(child.poll());time.sleep(.01)
                self.assertTrue(cp is not None and int(cp.read_text().split()[6])>0)
                child.terminate();self.assertEqual(child.wait(timeout=15),130,(self.root/'owned-route.log').read_text())
            finally:s.stop_owned(child)
        listed=subprocess.run(prefix+['runs','--store',str(self.store)],env=env,capture_output=True,text=True,timeout=15)
        self.assertEqual(listed.returncode,0,listed.stderr)
        row=json.loads(listed.stdout)['runs'][0];self.assertTrue(row['available'])
        result=subprocess.run(prefix+['resume','--store',str(self.store),'--run-root',str(self.directory),'--expected-state',row['state_sha256'],'--enable-experimental-vanity-local'],env=env,capture_output=True,text=True,timeout=20)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        state=c.inspect(self.store,self.directory,base.CLI,allow_complete=True)[0]['native']
        self.assertEqual((state['checked'],state['claims'],state['completions']),(c.TOTAL,1,1))

    def test_run_session_rejects_production_and_unsupported_options(self):
        import subprocess
        for args,code in [(['run','71','--engine','vanity'],1),(['run','20','--engine','vanity','--backend','cuda','--vanity-session','start','--store',str(self.store),'--run-root',str(self.directory)],2),(['run','20','--engine','vanity','--backend','cuda','--vanity-session','runs','--store',str(self.store),'--server','https://claudiodb.com'],2)]:
            result=subprocess.run([str(base.CLI),*args],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,code,result.stdout+result.stderr)
        self.assertFalse(self.directory.exists())

    def test_standalone_runtime_without_test_tree_stops_and_resumes(self):
        import subprocess
        import sys
        standalone=self.root/'standalone'/'measured'
        shutil.copytree(Path(c.__file__).parent,standalone,ignore=shutil.ignore_patterns('__pycache__'))
        self.assertFalse((standalone.parent/'tests').exists())
        tools=self.root/'standalone-tools';tools.mkdir();gpu=self.record['uuid']
        inventory=tools/'nvidia-smi'
        inventory.write_text('#!/usr/bin/env python3\nimport sys\nquery=sys.argv[1]\n'+
            'if query.startswith("--query-compute-apps"):pass\n'+
            'elif query=="--query-gpu=uuid,name,driver_version":print("'+gpu+', NVIDIA GeForce RTX 4070 SUPER, synthetic-driver")\n'+
            'elif query.startswith("--query-gpu=uuid,temperature.gpu"):print("'+gpu+',30,20,245,0,1000,1000,50")\n'+
            'else:raise SystemExit("Unexpected GPU write/query")\n')
        inventory.chmod(0o700)
        env=dict(os.environ,PATH=str(tools)+os.pathsep+os.environ['PATH'],PYTHONDONTWRITEBYTECODE='1')
        prefix=[sys.executable,str(standalone/'Coordinator.py')]
        common=['--cli',str(base.CLI),'--store',str(self.store)]
        with (self.root/'standalone.log').open('w') as log:
            child=subprocess.Popen(prefix+['start',*common,'--run-root',str(self.directory),'--enable-experimental-vanity-local'],env=env,stdout=log,stderr=log,start_new_session=True)
            try:
                deadline=time.monotonic()+15
                while time.monotonic()<deadline:
                    cp=next((self.directory/'native').glob('segment-*/checkpoint.txt'),None)
                    if cp and int(cp.read_text().split()[6])>0:break
                    self.assertIsNone(child.poll());time.sleep(.01)
                self.assertTrue(cp and int(cp.read_text().split()[6])>0)
                child.terminate();self.assertEqual(child.wait(timeout=15),130,(self.root/'standalone.log').read_text())
            finally:s.stop_owned(child)
        listed=subprocess.run(prefix+['runs',*common],env=env,capture_output=True,text=True,timeout=15)
        self.assertEqual(listed.returncode,0,listed.stderr)
        row=json.loads(listed.stdout)['runs'][0];self.assertTrue(row['available'],row)
        resume=prefix+['resume',*common,'--run-root',str(self.directory),'--expected-state',row['state_sha256'],'--enable-experimental-vanity-local']
        # The owned seal pins the runtime contract rather than a test file.
        contract=standalone/'LocalCoordinator.py';original=contract.read_bytes();before=self.snapshot()
        try:
            contract.write_bytes(original+b'\n# changed contract\n')
            refused=subprocess.run(resume,env=env,capture_output=True,text=True,timeout=15)
            self.assertEqual(refused.returncode,2);self.assertIn('Coordinator source differs',refused.stderr)
            self.assertEqual(self.snapshot(),before)
        finally:contract.write_bytes(original)
        result=subprocess.run(resume,env=env,capture_output=True,text=True,timeout=20)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        meta=e.read_json(self.directory/c.META);seal=e.read_json(self.directory/c.SEAL)
        self.assertEqual(meta['schema'],'openpuzzle_coordinator_desktop_v4')
        self.assertEqual(meta['contract_sha256'],s.sha(contract))
        self.assertEqual((seal['native']['checked'],seal['native']['claims'],seal['native']['completions']),(c.TOTAL,1,1))

if __name__=='__main__':unittest.main(verbosity=2)
