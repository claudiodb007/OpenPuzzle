#!/usr/bin/env python3
"""Local HTTP, native adapter, shared monitor/export and CLI regression checks."""
import hashlib,json,os,signal,socket,subprocess,sys,tempfile,threading,time,unittest
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent/'support'))
import Support as s
from LocalScenarios import wif,descendant
CLI=Path(sys.argv[1]).resolve();ADAPTER=Path(sys.argv[2]).resolve();sys.argv=sys.argv[:1]
ENGINE=Path(os.environ.get('OPV_HTTP_REAL_ENGINE',Path(__file__).with_name('FakeTargetEngine.py'))).resolve()
UUID=os.environ.get('OPV_HTTP_REAL_UUID','GPU-11111111-1111-1111-1111-111111111111')
REAL='OPV_HTTP_REAL_ENGINE' in os.environ

class Fixture:
    def __init__(self,d,kind='normal',solution=False,target_kind='p2pkh'):
        self.d=d;self.kind=kind;self.solution=solution;self.requests=[];self.errors=[];self.failures=0;self.complete_attempts=0;self.report_attempts=0;self.progress=0;self.claims=0
        self.count=(32 if REAL else 1)*256*1024*8+7;self.grid=32 if REAL else 1
        if REAL:self.count=2**34+7
        self.key=s.BASE if solution else s.BASE-1;self.hash=s.key_hash(self.key);self.address=s.address(self.hash)
        (d/'controls').mkdir();(d/'home').mkdir();self.target_kind=target_kind
        fixture=self
        class Handler(BaseHTTPRequestHandler):
            protocol_version='HTTP/1.1'
            def log_message(self,*_):pass
            def do_POST(self):
                try:
                    body=self.rfile.read(int(self.headers['Content-Length'])).decode();data=json.loads(body);fixture.requests.append((self.path,data))
                    assert f'{fixture.key:064x}' not in body and wif(fixture.key) not in body and 'private' not in body and str(fixture.d) not in body
                    code=200;result={'success':True};extra=[]
                    if self.path=='/api/range/claim':
                        fixture.claims+=1;assert fixture.claims==1
                        assert data['puzzle']==20 and data['backend']=='cuda' and data['execution_slot']=='cuda-0'
                        result=dict(assignment_id='http-local-a',puzzle=20,range_id=1,target=fixture.address,search_mode='linear',required_backend='cuda',start=f'{s.BASE:064x}',end=f'{s.BASE+fixture.count-1:064x}')
                        if fixture.kind=='wrong-target':result['target']=s.address(s.key_hash(s.BASE+42))
                        if fixture.kind=='wrong-range':result['end']=f'{s.BASE+fixture.count:064x}'
                        if fixture.kind=='bad-id':result['assignment_id']='../escape'
                        if fixture.kind=='malformed':result='broken'
                        if fixture.kind=='duplicate':result=json.dumps(result)[:-1]+',"puzzle":20}'
                        if fixture.kind=='trailing':result=json.dumps(result)+' invalid'
                        if fixture.kind=='redirect':code=302;extra=[('Location','http://192.0.2.1:9999/api/range/claim')]
                    elif self.path=='/api/range/progress':
                        n=int(data['keys_checked']);assert fixture.progress<=n<=fixture.count;fixture.progress=n
                        if fixture.kind=='reject-before' or (fixture.kind=='reject-during' and n>0):code=409
                        if fixture.kind=='progress-fail' and n>0:code=503
                        if fixture.kind=='progress-once' and n>0 and fixture.failures==0:code=503;fixture.failures+=1
                        if n>0 and fixture.kind=='safe-stop':(d/'controls/safe-stop').write_text('stop\n')
                        if n>0 and fixture.kind=='cancel':(d/'controls/cancel').write_text('cancel\n')
                    elif self.path=='/api/range/complete':
                        fixture.complete_attempts+=1;assert not fixture.solution and int(data['keys_checked'])==fixture.count and data['exit_code']==0 and data['status']=='completed'
                        if fixture.kind=='complete-fail':code=503
                        if fixture.kind=='complete-once' and fixture.complete_attempts==1:code=503
                        if fixture.kind=='lost-ack' and fixture.complete_attempts==1:self.close_connection=True;self.connection.shutdown(socket.SHUT_RDWR);return
                    elif self.path=='/api/puzzle/report-solution':
                        fixture.report_attempts+=1;assert fixture.solution and set(data)=={'assignment_id','client_id'}
                        if fixture.kind=='report-fail':code=503
                        if fixture.kind=='report-once' and fixture.report_attempts==1:code=503
                    else:raise AssertionError('Unexpected endpoint '+self.path)
                    if code!=200:result={'error':'fixture-refused'}
                    payload=(result if isinstance(result,str) else json.dumps(result,separators=(',',':'))).encode()
                    self.send_response(code)
                    for k,v in extra:self.send_header(k,v)
                    self.send_header('Content-Length',str(len(payload)+(1 if fixture.kind=='truncated' else 0)))
                    if fixture.kind=='duplicate-header':self.send_header('Content-Length',str(len(payload)))
                    if fixture.kind=='chunked':self.send_header('Transfer-Encoding','chunked')
                    self.send_header('Connection','close');self.end_headers();self.wfile.write(payload);self.close_connection=True
                except (BrokenPipeError,ConnectionResetError):pass
                except Exception as e:fixture.errors.append(repr(e));self.close_connection=True
        self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler);self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()
        self.url=f'http://127.0.0.1:{self.server.server_port}'
    def args(self):
        options=dict(engine=ENGINE,engine_sha256=s.sha(ENGINE),adapter=ADAPTER,adapter_sha256=s.sha(ADAPTER),root=self.d/'state',controls=self.d/'controls',server=self.url,start=f'{s.BASE:x}',count=self.count,grid=self.grid,target_kind=self.target_kind,target=self.address if self.target_kind=='p2pkh' else self.hash.hex(),gpu_uuid=UUID)
        return [str(CLI),'vanity-preview','http-test',*[str(x) for k,v in options.items() for x in ('--'+k.replace('_','-'),v)],'--enable-experimental-vanity-local']
    def call(self,kill=False):
        env=os.environ.copy();env.update(HOME=str(self.d/'home'),XDG_CONFIG_HOME=str(self.d/'home/.config'),XDG_DATA_HOME=str(self.d/'home/.local/share'),CUDA_VISIBLE_DEVICES=UUID)
        if REAL:s.require_idle(UUID)
        limit=s.telemetry(UUID)['power_limit_w'] if REAL else None
        log=self.d/f'call-{len(list(self.d.glob("call-*.log")))}.log';started=time.monotonic();sample_at=0
        with log.open('w') as f:
            p=subprocess.Popen(self.args(),env=env,stdout=f,stderr=subprocess.STDOUT,start_new_session=True)
            try:
                while p.poll() is None:
                    now=time.monotonic()
                    if now-started>90:raise RuntimeError('Local fixture timed out')
                    if REAL and now>=sample_at:
                        sample=s.telemetry(UUID);sample_at=now+1
                        if sample['temperature_c']>=85 or abs(sample['power_limit_w']-limit)>.5:raise RuntimeError('GPU temperature/power limit changed')
                        if any(not descendant(pid,p.pid) for pid in s.compute_pids(UUID)):raise RuntimeError('GPU used by another process')
                    if kill and 0<self.progress<self.count:p.kill();kill=False
                    time.sleep(.01)
            finally:s.stop_owned(p)
        if self.errors:raise AssertionError(self.errors)
        return p.returncode,log.read_text()
    def state(self):return (self.d/'state/http-state.txt').read_text().splitlines()[2].split()
    def close(self):
        self.server.shutdown();self.server.server_close();self.thread.join()
        (self.d/'local-http-requests.json').write_text(json.dumps(self.requests,indent=2)+'\n')

class Tests(unittest.TestCase):
    def setUp(self):
        parent=os.environ.get('OPV_HTTP_REPORT');self.temp=None
        if parent:self.d=Path(parent)/self._testMethodName;self.d.mkdir()
        else:self.temp=tempfile.TemporaryDirectory(prefix='opv-http-');self.d=Path(self.temp.name)
        self.fixtures=[]
    def tearDown(self):
        for f in self.fixtures:f.close()
        if self.temp:self.temp.cleanup()
    def fixture(self,*a,**kw):f=Fixture(self.d,*a,**kw);self.fixtures.append(f);return f
    def expect(self,f,code):
        actual,log=f.call();self.assertEqual(actual,code,log);return log
    def test_normal_and_repeat(self):
        f=self.fixture();self.expect(f,0);self.assertEqual(f.state()[0],'done');self.assertEqual(f.state()[3],'1');self.expect(f,20);self.assertEqual(f.claims,1)
    def test_hash_target(self):self.expect(self.fixture(target_kind='hash160'),0)
    def test_complete_retry(self):
        f=self.fixture('complete-once');self.expect(f,0);self.assertEqual(f.complete_attempts,2);self.assertEqual(f.state()[3],'1')
    def test_lost_ack(self):
        f=self.fixture('lost-ack');self.expect(f,0);self.assertEqual(f.complete_attempts,2)
    def test_restart_pending_completion(self):
        f=self.fixture('complete-fail');self.expect(f,2);self.assertEqual(f.state()[0],'pending-complete');f.kind='normal';self.expect(f,0);self.assertEqual(f.state()[3],'1');self.assertEqual(f.claims,1)
    def test_progress_retry(self):self.expect(self.fixture('progress-once'),0)
    def test_progress_failure_resume(self):
        f=self.fixture('progress-fail');self.expect(f,3);f.kind='normal';self.expect(f,0);self.assertEqual(f.claims,1)
    def test_reject_before_kernel(self):
        f=self.fixture('reject-before');self.expect(f,2);self.assertEqual(f.state()[3],'0');self.expect(f,20)
    def test_reject_during_kernel(self):
        f=self.fixture('reject-during');self.expect(f,2);self.expect(f,20);self.assertEqual(f.complete_attempts,0)
    def test_cancel_resume(self):
        f=self.fixture('cancel');self.expect(f,130);(self.d/'controls/cancel').unlink();f.kind='normal';self.expect(f,0);self.assertEqual(f.claims,1)
    def test_owner_crash_resume(self):
        f=self.fixture();code,log=f.call(kill=True);self.assertEqual(code,-9,log);time.sleep(.5);self.expect(f,0);self.assertEqual(f.claims,1)
    def test_safe_stop(self):
        f=self.fixture('safe-stop');self.expect(f,0);self.expect(f,20);self.assertEqual(f.claims,1)
    def test_solution_and_report_retry(self):
        f=self.fixture('report-once',solution=True);log=self.expect(f,10);self.assertEqual(f.report_attempts,2);self.assertEqual(f.complete_attempts,0);self.assertNotIn(wif(s.BASE),log)
        wallet=self.d/'home/OpenPuzzle-Solutions/Puzzle-20/http-local-a/wallet-import.txt';self.assertIn(wif(s.BASE),wallet.read_text());self.assertEqual(wallet.stat().st_mode&0o077,0);self.expect(f,20)
    def test_solution_report_recovery(self):
        f=self.fixture('report-fail',solution=True);self.expect(f,2);self.assertEqual(f.state()[0],'solution');f.kind='normal';self.expect(f,10);self.assertEqual(f.state()[3],'1');self.assertEqual(f.claims,1)
    def test_wrong_target(self):
        f=self.fixture('wrong-target');self.expect(f,2);self.assertEqual(f.state()[3],'0');self.expect(f,20)
    def test_wrong_range(self):self.expect(self.fixture('wrong-range'),2)
    def test_unsafe_id(self):self.expect(self.fixture('bad-id'),2)
    def test_malformed_claim(self):self.expect(self.fixture('malformed'),2)
    def test_duplicate_claim_fields(self):self.expect(self.fixture('duplicate'),2)
    def test_trailing_claim_garbage(self):self.expect(self.fixture('trailing'),2)
    def test_truncated_http_body(self):self.expect(self.fixture('truncated'),2)
    def test_duplicate_http_length(self):self.expect(self.fixture('duplicate-header'),2)
    def test_chunked_reply_refused(self):self.expect(self.fixture('chunked'),2)
    def test_redirect_refused(self):
        f=self.fixture('redirect');self.expect(f,2);self.assertEqual(len(f.requests),1)
    def test_remote_endpoints_refused_before_state(self):
        f=self.fixture()
        for url in ['https://127.0.0.1:4444','http://localhost:4444','http://127.0.0.1:4444/path','http://example.com:4444','http://127.0.0.1:80']:
            f.url=url;self.expect(f,2);self.assertFalse((self.d/'state').exists())

if __name__=='__main__':
    os.umask(0o077);ENGINE.chmod(ENGINE.stat().st_mode|0o100) if not REAL else None
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Tests))
    if os.environ.get('OPV_HTTP_REPORT'):
        Path(os.environ['OPV_HTTP_REPORT'],'http-results.json').write_text(json.dumps(dict(tests=result.testsRun,passed=result.wasSuccessful(),failures=[test.id() for test,_ in result.failures],errors=[test.id() for test,_ in result.errors],real_engine=REAL),indent=2)+'\n')
    sys.exit(0 if result.wasSuccessful() else 1)
