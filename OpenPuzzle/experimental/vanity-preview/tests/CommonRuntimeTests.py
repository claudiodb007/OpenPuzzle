"""Real common C++ factory/builder/launcher/state/HTTP/sync; synthetic engine only."""
import contextlib
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import uuid

DRIVER=Path(sys.argv.pop(1)).resolve();ADAPTER=Path(sys.argv.pop(1)).resolve()
sys.path.insert(0,str(Path(__file__).parent/'support'))
import Support as support


class Peer:
    def __init__(self,test):
        self.calls=[];self.test=test;self.fail_complete=False;self.reject=False;self.checked=0
        peer=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_):pass
            def do_POST(self):
                q=json.loads(self.rfile.read(int(self.headers['Content-Length'])));peer.calls.append((self.path,q))
                if self.path=='/api/range/claim':
                    test.assertEqual(q,dict(client_id=test.client,puzzle=65,execution_slot='primary',backend='cuda',target_duration_minutes=1,speed_mkeys=2500))
                    result=dict(assignment_id=test.assignment,puzzle=65,range_id=37,target=test.target,start=f'{test.first:x}',end=f'{test.first+test.count-1:x}',search_mode='linear');code=200
                elif self.path=='/api/range/progress':
                    test.assertEqual(set(q),{'assignment_id','client_id','status','speed_mkeys','keys_checked'})
                    test.assertEqual((q['assignment_id'],q['client_id'],q['status']),(test.assignment,test.client,'running'))
                    test.assertGreaterEqual(int(q['keys_checked']),peer.checked);test.assertLessEqual(int(q['keys_checked']),test.count)
                    peer.checked=int(q['keys_checked']);result=dict(success=True,status='running',keys_checked=q['keys_checked'],speed_mkeys=q['speed_mkeys']);code=200
                    if peer.reject:result=dict(error='assignment_lease_expired',message='Lease expired');code=409
                elif self.path=='/api/range/complete':
                    test.assertEqual(q['assignment_id'],test.assignment);test.assertEqual(q['client_id'],test.client)
                    test.assertEqual(q['status'],'completed');test.assertEqual(q['exit_code'],0);test.assertEqual(int(q['keys_checked']),test.count)
                    result=dict(success=True,status='completed',already_finalized=True);code=200
                    if peer.fail_complete:result=dict(error='temporary_unavailable');code=503
                else:result=dict(error='unexpected_endpoint');code=404
                data=json.dumps(result).encode();self.send_response(code);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
        self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler);self.server.daemon_threads=True
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()
        self.url='http://127.0.0.1:'+str(self.server.server_port)
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join(timeout=5)


class Tests(unittest.TestCase):
    def setUp(self):
        os.umask(0o077);self.tmp=tempfile.TemporaryDirectory(prefix="opv shared ç ' $() ");self.root=Path(self.tmp.name)
        self.home=self.root/'home';self.home.mkdir();self.workspace=self.root/'assignment';self.workspace.mkdir()
        self.engine_dir=self.root/'engine';self.engine_dir.mkdir()
        shutil.copytree(Path(__file__).parent/'support',self.engine_dir/'support')
        self.engine=self.engine_dir/'FakeTargetEngine.py'
        self.engine.write_text((Path(__file__).parent/'FakeTargetEngine.py').read_text().replace('time.sleep(.12)','time.sleep(.01)'));self.engine.chmod(0o700)
        self.first=support.BASE;self.count=2**30;self.target=support.address(support.key_hash(support.BASE-1))
        self.client=str(uuid.uuid4());self.assignment='8e9621a2-228d-4447-8126-e8aa000f9b2e';self.peer=Peer(self);self.pid=None
        self.env={k:v for k,v in os.environ.items() if not k.startswith(('OPENPUZZLE_','OPV_','CUDA_','GIT_'))}
        self.env.update(HOME=str(self.home),XDG_CONFIG_HOME=str(self.home/'.config'),XDG_DATA_HOME=str(self.home/'.local/share'),PYTHONDONTWRITEBYTECODE='1')
    def tearDown(self):
        if self.pid:
            try:os.killpg(self.pid,signal.SIGTERM)
            except ProcessLookupError:pass
        self.peer.close();self.tmp.cleanup()
    def args(self,mode):
        return [str(DRIVER),mode,str(self.workspace),str(self.count),str(self.engine),str(ADAPTER),
            hashlib.sha256(self.engine.read_bytes()).hexdigest(),hashlib.sha256(ADAPTER.read_bytes()).hexdigest(),
            f'{self.first:x}',f'{self.first+self.count-1:x}',self.target,self.peer.url,self.client]
    def command(self,args,code=0):
        result=subprocess.run(args,env=self.env,capture_output=True,text=True,timeout=25)
        diagnostic=(self.workspace/'bitcrack.log').read_text() if (self.workspace/'bitcrack.log').exists() else ''
        self.assertEqual(result.returncode,code,result.stdout+result.stderr+diagnostic[-3000:]);return result
    def start(self,mode='start'):
        result=self.command(self.args(mode));self.pid=json.loads(result.stdout)['pid']
    def finished(self):
        deadline=time.monotonic()+12
        while not (self.workspace/'exit.code').exists() and time.monotonic()<deadline:time.sleep(.01)
        self.assertTrue((self.workspace/'exit.code').exists(),(self.workspace/'bitcrack.log').read_text())
        return int((self.workspace/'exit.code').read_text())
    def inspect(self):return json.loads(self.command([str(DRIVER),'inspect',str(self.workspace),str(self.count)]).stdout)
    def sync(self):return json.loads(self.command([str(DRIVER),'sync',str(self.workspace),str(self.count),self.peer.url]).stdout)
    def digest(self):return {str(p.relative_to(self.workspace)):hashlib.sha256(p.read_bytes()).hexdigest() for p in self.workspace.rglob('*') if p.is_file()}
    def wait_progress(self):
        cp=self.workspace/'vanity-native/checkpoint.txt';deadline=time.monotonic()+10
        while time.monotonic()<deadline:
            if cp.is_file() and int(cp.read_text().split()[6])>0:return
            time.sleep(.005)
        self.fail('No durable native progress')

    def test_shared_catalog_parser_and_fail_closed_records(self):self.command([str(DRIVER),'parser'])

    def test_real_common_claim_launch_progress_completion_and_state_cleanup(self):
        self.count=2**35;self.start('claim-start');self.wait_progress();self.assertTrue(self.sync()['progress'])
        self.assertEqual(self.finished(),0)
        value=self.inspect();self.assertTrue(value['proof']);self.assertEqual(value['checked'],str(self.count))
        result=self.sync();self.assertTrue(result['uploaded']);self.assertTrue(result['removed'])
        self.assertEqual([path for path,_ in self.peer.calls],['/api/range/claim','/api/range/progress','/api/range/complete'])
        self.assertFalse((self.workspace/'peer').exists());self.assertFalse((self.workspace/'http-live.json').exists())

    def test_native_interruption_rebuild_resumes_exact_prefix_without_another_claim(self):
        source=self.engine.read_text().replace("    print('OPV1 PROGRESS',checked,flush=True)",
            "    print('OPV1 PROGRESS',checked,flush=True)\n    if first==support_base_for_hold and checked==2*batch:\n        deadline=time.monotonic()+10\n        while not stopped and time.monotonic()<deadline:time.sleep(.001)\n        assert stopped, 'Expected fixture stop'")
        source=source.replace('support_base_for_hold','s.BASE');self.engine.write_text(source)
        self.count=2**35;self.start('claim-start');self.wait_progress()
        attempt=next((self.workspace/'vanity-native').glob('attempt-*'))
        native_pid=int(attempt.name.split('-')[1]);os.kill(native_pid,signal.SIGTERM)
        self.assertNotEqual(self.finished(),0)
        before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (self.workspace/'vanity-native').glob('attempt-*/engine.log')}
        self.assertFalse(self.inspect()['proof']);(self.workspace/'exit.code').unlink()
        self.start();self.assertEqual(self.finished(),0);self.assertTrue(self.inspect()['proof']);self.assertTrue(self.sync()['removed'])
        for name,digest in before.items():self.assertEqual(hashlib.sha256(Path(name).read_bytes()).hexdigest(),digest)
        self.assertEqual(sum(p=='/api/range/claim' for p,_ in self.peer.calls),1)

    def test_progress_above_native_checkpoint_is_not_uploaded(self):
        self.start();self.assertEqual(self.finished(),0)
        cp=self.workspace/'vanity-native/checkpoint.txt';text=cp.read_text();cp.write_text(text.replace(str(self.count)+' complete',str(self.count-1)+' interrupted'))
        self.assertFalse(self.inspect()['progress']);self.assertFalse(self.inspect()['proof'])

    def test_lost_completion_retry_uses_existing_idempotent_endpoint_without_new_launch(self):
        self.start('claim-start');self.assertEqual(self.finished(),0);before=self.digest();self.peer.fail_complete=True
        result=self.sync();self.assertFalse(result['removed']);self.peer.fail_complete=False
        result=self.sync();self.assertTrue(result['uploaded']);self.assertEqual(before,self.digest())
        self.assertEqual(sum(p=='/api/range/claim' for p,_ in self.peer.calls),1)
        self.assertEqual(sum(p=='/api/range/complete' for p,_ in self.peer.calls),2)

    def test_native_checkpoint_required_even_with_shared_terminal_log(self):
        self.start();self.assertEqual(self.finished(),0)
        cp=self.workspace/'vanity-native/checkpoint.txt';cp.write_text(cp.read_text().replace('complete','interrupted'))
        self.assertFalse(self.inspect()['proof'])

    def test_log_corruption_or_terminal_tail_never_proves_completion(self):
        self.start();self.assertEqual(self.finished(),0);log=self.workspace/'vanity.log'
        with log.open('a') as stream:stream.write('OPV_CLIENT PROGRESS 1 elapsed_us=1\n')
        self.assertFalse(self.inspect()['proof'])

    def test_missing_native_attempt_proof_never_completes(self):
        self.start();self.assertEqual(self.finished(),0)
        log=next((self.workspace/'vanity-native').glob('attempt-*/engine.log'));log.rename(log.with_suffix('.preserved'))
        self.assertFalse(self.inspect()['proof'])

    def test_changed_engine_hash_and_assignment_bindings_are_refused(self):
        self.command(self.args('build'));before=self.digest();args=self.args('build');args[6]='0'*64
        self.command(args,code=2);self.assertEqual(before,self.digest())
        args=self.args('build');args[8]=f'{self.first+1:x}';self.command(args,code=2)
        # The common target builder may rewrite its public targets.txt; native evidence stays identical.
        self.assertEqual(before,self.digest())

    def test_private_solution_uses_common_result_and_wallet_export(self):
        self.target=support.address(support.key_hash(support.BASE));self.start();self.assertEqual(self.finished(),10)
        self.assertTrue((self.workspace/'found.txt').is_file());self.assertFalse(self.inspect()['proof'])
        result=self.sync();self.assertTrue(result['solution']);self.assertFalse(result['removed']);self.assertEqual(self.peer.calls,[])
        result=self.command([str(DRIVER),'export',str(self.workspace),str(self.count)]);self.assertTrue(json.loads(result.stdout)['success'])
        for p in self.workspace.rglob('*.txt'):self.assertEqual(p.stat().st_mode&0o077,0)

    def test_interval_above_native_limit_refused_before_native_directory(self):
        self.count=2**40+1;self.command(self.args('build'),code=2);self.assertFalse((self.workspace/'vanity-native').exists())

    def test_wrong_assignment_total_does_not_accept_another_terminal(self):
        self.start();self.assertEqual(self.finished(),0);self.count+=1;self.assertFalse(self.inspect()['proof'])


if __name__=='__main__':unittest.main(verbosity=2)
