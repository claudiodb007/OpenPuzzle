#!/usr/bin/env python3
"""Real native CLI/adapter with a synthetic engine; never requires a GPU."""
import contextlib
import fcntl
import io
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import threading
import time
import unittest
import uuid
from unittest.mock import patch
import MeasuredProfileTests as base
import Profile as p
import Resume as r
import Evidence as e
import Support as s


class Tests(unittest.TestCase):
    def setUp(self):
        base.Tests.setUp(self)
        # Short native synthetic runs. Only the test copy is changed.
        directory=self.root/'fake-engine';directory.mkdir()
        shutil.copytree(self.engine.parent/'support',directory/'support')
        engine=directory/self.engine.name
        engine.write_text(self.engine.read_text().replace('time.sleep(.12)','time.sleep(.003)'));engine.chmod(0o700)
        self.paths['engine']=engine
        self.proof['executables']['engine']=dict(path=str(engine),sha256=s.sha(engine))
        s.write_json(self.evidence,self.proof);self.record=e.profile(self.proof,self.evidence,self.paths);s.write_json(self.candidate,self.record)
        p.import_profile(self.candidate,self.store)
        self.directory=self.store/('run-'+str(uuid.uuid4()))
        self.original_server=p.m.LocalMeasurementServer

    tearDown=base.Tests.tearDown

    @contextlib.contextmanager
    def hardware(self,kind=None):
        threads=[]
        def server(count,address,**kwargs):
            server=self.original_server(count,address,**kwargs)
            if kind:
                initial=kwargs.get('initial_checked',0)
                def trigger():
                    deadline=time.monotonic()+20
                    while time.monotonic()<deadline:
                        if server.progress and server.progress[-1]>=initial+2*r.BATCH:
                            (self.directory/'controls'/kind).touch(mode=0o600);return
                        time.sleep(.001)
                t=threading.Thread(target=trigger,daemon=True);threads.append(t);t.start()
            return server
        with patch.object(p,'hardware',return_value=(base.UUID,245)),patch.object(s,'telemetry',return_value=dict(temperature_c=30,power_limit_w=245)),patch.object(s,'compute_pids',return_value=[]),patch.object(p.m,'LocalMeasurementServer',side_effect=server),contextlib.redirect_stdout(io.StringIO()):
            yield
        for t in threads:t.join(timeout=1)

    def cancelled(self):
        with self.hardware('cancel'):self.assertEqual(r.new_test(self.store,self.directory),130)
        return e.read_json(self.directory/r.SEAL)

    def resume(self,kind=None):
        with self.hardware(kind):return r.resume(self.store,self.directory,s.sha(self.directory/r.SEAL))

    def snapshot(self):
        return {str(f.relative_to(self.directory)):s.sha(f) for f in self.directory.rglob('*') if f.is_file()}

    def test_cancel_resume_exact_completion_no_new_claim(self):
        before=self.cancelled();old=self.snapshot()
        self.assertEqual(self.resume(),0)
        after=e.read_json(self.directory/r.SEAL)
        self.assertEqual(after['checked'],r.TOTAL);self.assertEqual(after['launches'],2)
        calls=e.read_json(self.directory/'http-requests.json')
        self.assertEqual(sum(x['path'].endswith('/claim') for x in calls),1)
        self.assertEqual(sum(x['path'].endswith('/complete') for x in calls),1)
        histories=list((self.directory/'resume-history').iterdir());self.assertEqual(len(histories),1)
        for name,digest in before['files'].items():self.assertEqual(s.sha(histories[0]/'before'/name),digest)
        self.assertEqual(s.sha(histories[0]/'before'/r.SEAL),old[r.SEAL])
        with self.assertRaises(RuntimeError):r.inspect(self.store,self.directory)

    def test_repeated_cancel_then_finish_has_contiguous_native_intervals(self):
        first=self.cancelled()
        self.assertEqual(self.resume('cancel'),130)
        second=e.read_json(self.directory/r.SEAL)
        self.assertGreater(second['checked'],first['checked']);self.assertEqual(second['launches'],2)
        self.assertEqual(self.resume(),0)
        after=e.read_json(self.directory/r.SEAL);self.assertEqual(after['launches'],3)
        r.protocol_coverage(self.directory,r.TOTAL,3)

    def test_safe_stop_during_resume_finishes_and_blocks_replay(self):
        self.cancelled();self.assertEqual(self.resume('safe-stop'),0)
        after=e.read_json(self.directory/r.SEAL);self.assertTrue(after['safe_stop']);self.assertEqual(after['checked'],r.TOTAL)
        before=self.snapshot()
        with self.assertRaises(RuntimeError):self.resume()
        self.assertEqual(before,self.snapshot())

    def test_busy_port_refuses_before_any_run_mutation(self):
        self.cancelled();argv=e.read_json(self.directory/'argv.json');url=argv[argv.index('--server')+1]
        with socket.socket() as sock:
            sock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);sock.bind(('127.0.0.1',int(url.rsplit(':',1)[1])));sock.listen()
            before=self.snapshot()
            with self.assertRaises(OSError):self.resume()
            self.assertEqual(before,self.snapshot())
        self.assertEqual(self.resume(),0)

    def test_changed_checkpoint_and_confirmation_refused(self):
        self.cancelled();before=self.snapshot()
        with self.assertRaises(RuntimeError):r.resume(self.store,self.directory,'0'*64)
        self.assertEqual(before,self.snapshot())
        cp=self.directory/'state/assignment/checkpoint.txt';cp.write_text(cp.read_text().replace('interrupted','complete'))
        damaged=self.snapshot()
        with self.assertRaises(RuntimeError):self.resume()
        self.assertEqual(damaged,self.snapshot())

    def test_spawn_failure_restores_original_cancel_checkpoint_and_can_retry(self):
        self.cancelled();before=e.read_json(self.directory/r.SEAL)
        with patch.object(r.subprocess,'Popen',side_effect=OSError('synthetic failed spawn')):
            with self.assertRaises(OSError):self.resume()
        self.assertFalse((self.directory/'resume-active.json').exists())
        self.assertEqual(r.inspect(self.store,self.directory)[0],before)
        self.assertEqual(self.resume(),0)

    def test_busy_gpu_preserves_all_run_files(self):
        self.cancelled();before=self.snapshot()
        with patch.object(p,'hardware',side_effect=RuntimeError('busy')):
            with self.assertRaises(RuntimeError):r.resume(self.store,self.directory,s.sha(self.directory/r.SEAL))
        self.assertEqual(before,self.snapshot())

    def test_incomplete_resume_transaction_is_not_automatically_restarted(self):
        self.cancelled();s.write_json(self.directory/'resume-active.json',{'interrupted':True});before=self.snapshot()
        with self.assertRaises(RuntimeError):self.resume()
        self.assertEqual(before,self.snapshot())

    def test_unowned_or_symlink_run_refused(self):
        self.cancelled();link=self.store/('run-'+str(uuid.uuid4()));link.symlink_to(self.directory,target_is_directory=True)
        with self.assertRaises(RuntimeError):r.inspect(self.store,link)
        with self.assertRaises(RuntimeError):r.inspect(self.root,self.directory)

    def test_listing_reopen_and_native_cli_optin(self):
        self.cancelled()
        result=subprocess.run([str(base.CLI),'vanity-preview','measured-runs','--store',str(self.store)],capture_output=True,text=True,timeout=15)
        self.assertEqual(result.returncode,0,result.stderr);row=json.loads(result.stdout)['runs'][0]
        self.assertTrue(row['available']);self.assertEqual(row['path'],str(self.directory))
        before=self.snapshot()
        args=[str(base.CLI),'vanity-preview','measured-resume','--store',str(self.store),'--run-root',str(self.directory),'--expected-state',row['state_sha256']]
        result=subprocess.run(args,capture_output=True,text=True,timeout=15)
        self.assertEqual(result.returncode,2);self.assertEqual(before,self.snapshot())

    def test_source_history_change_and_solution_refused(self):
        self.cancelled();attempt=next((self.directory/'state/assignment').glob('attempt-*'));result=attempt/'solutions.txt'
        result.write_text('synthetic unexpected match')
        before=self.snapshot()
        with self.assertRaises(RuntimeError):self.resume()
        self.assertEqual(before,self.snapshot())

    def test_native_owner_lock_blocks_resume(self):
        self.cancelled();fd=os.open(self.directory/'state/owner.lock',os.O_RDONLY)
        try:
            fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB);before=self.snapshot()
            with self.assertRaises(BlockingIOError):self.resume()
            self.assertEqual(before,self.snapshot())
        finally:os.close(fd)


if __name__=='__main__':unittest.main(verbosity=2)
