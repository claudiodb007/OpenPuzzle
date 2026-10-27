"""Actual native RunSession/adapter + synthetic engine + isolated receipt peer."""
import copy
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import unittest
CLI=Path(sys.argv.pop(1)).resolve();ADAPTER=Path(sys.argv.pop(1)).resolve()
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'measured'))
from ReceiptCoordinator import ReceiptAssignmentServer
from ServerContract import PROTOCOL, encoded
import Support as s
UUID='GPU-11111111-2222-3333-4444-555555555555'
ENGINE=Path(__file__).with_name('FakeTargetEngine.py').resolve()
BATCH=256*1024


class Fixture(ReceiptAssignmentServer):
    def __init__(self,root,ledger_path,kind='normal',target=None,port=0,client_id=None,resume_only=False):
        self.kind=kind;self.used=False;self.boundary=threading.Event();self.release=threading.Event()
        self.intent_checks=0
        super().__init__(root,ledger_path=ledger_path,cli=CLI,adapter=ADAPTER,engine=ENGINE,
            count=5*BATCH+7,grid=1,segment=2*BATCH,target=target or s.address(s.key_hash(s.BASE-1)),
            planning=2519.41,gpu_uuid=UUID,port=port,client_id=client_id,resume_only=resume_only)

    def handle(self,path,body):
        if path.endswith('/operations'):
            journal=json.loads((self.root/'receipt-client.json').read_text())
            last=journal['entries'][str(len(journal['entries'])-1)]
            assert last['request']==body and last['receipt'] is None
            assert last['request_sha256']==hashlib.sha256(encoded(body).encode()).hexdigest()
            self.intent_checks+=1
            if self.kind=='unknown-claim' and body['kind']=='claim' and not self.used:
                self.used=True;return 503,dict(error='synthetic_unavailable_before_effect')
        code,response=super().handle(path,body)
        if path.endswith('/query') and self.kind=='query-mismatch':
            response['client_id']='other-client'
        return code,response

    def respond(self,handler,code,response):
        kind=response.get('kind')
        drop_kind={'lost-claim':'claim','lost-progress':'progress','lost-complete':'complete'}.get(self.kind)
        if self.kind=='lost-progress' and kind=='progress' and response['assignment']['keys_checked']=='0':drop_kind=None
        if drop_kind is not None and not self.used and kind==drop_kind:
            self.used=True;handler.close_connection=True
            try:handler.connection.shutdown(socket.SHUT_RDWR)
            except OSError:pass
            return
        if not self.used and self.kind in ('kill-claim','kill-complete') and kind==self.kind[5:]:
            self.used=True;self.boundary.set()
            if not self.release.wait(10):raise RuntimeError('Synthetic process boundary timeout')
        if self.kind=='lease-before' and kind=='claim':
            import time
            self.ledger.clock=lambda:int(time.time())+400
        if self.kind=='tamper-claim' and kind=='claim':
            response=copy.deepcopy(response);response['assignment']['scope']['target']='1BitcoinEaterAddressDontSendf59kuE'
        if self.kind=='wrong-types' and kind=='claim':
            response=copy.deepcopy(response);response['assignment']['lease_epoch']=True
        if self.kind in ('cancel','safe') and kind=='progress' and int(response['assignment']['keys_checked'])>0:
            (self.root/'controls'/('cancel' if self.kind=='cancel' else 'safe-stop')).touch(mode=0o600)
        try:super().respond(handler,code,response)
        except (BrokenPipeError,ConnectionResetError):pass  # Only the owned test child may have been killed.

    def env(self):
        return dict(os.environ,CUDA_VISIBLE_DEVICES=UUID,HOME=str(self.root/'home'),
            XDG_CONFIG_HOME=str(self.root/'home/.config'),XDG_DATA_HOME=str(self.root/'home/.local/share'),
            OPENPUZZLE_EXECUTION_SLOT='cuda-0')

    def state(self):return (self.root/'coordinator-state.txt').read_text().splitlines()[2].split()
    def kernels(self):return list(self.root.glob('segment-*/attempt-*/engine.log'))
    def journal(self):return json.loads((self.root/'receipt-client.json').read_text())
    def operations(self,kind):return [r for r in self.requests if r['path'].endswith('/operations') and r['payload']['kind']==kind]
    def queries(self):return [r for r in self.requests if r['path'].endswith('/query')]


class Tests(unittest.TestCase):
    def setUp(self):
        os.umask(0o077);self.tmp=tempfile.TemporaryDirectory(prefix="opv receipts ç ' ")
        self.root=Path(self.tmp.name);self.fixtures=[]
    def tearDown(self):
        for f in self.fixtures:f.release.set();f.close()
        self.tmp.cleanup()
    def fixture(self,kind='normal',**kwargs):
        base=self.root/str(len(self.fixtures));base.mkdir();root=base/'native';root.mkdir();(root/'home').mkdir()
        peer=base/'peer';peer.mkdir();f=Fixture(root,peer/'ledger.sqlite',kind,**kwargs);self.fixtures.append(f);return f
    def run_cli(self,f,code=0,resume=False,args=None):
        result=subprocess.run(args or f.command(resume),env=f.env(),capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,code,result.stdout+'\n'+result.stderr)
        self.assertFalse(f.errors,f.errors);return result
    def complete(self,f):
        self.assertEqual(f.state()[0],'done');self.assertEqual(f.stats()['claim'],1);self.assertEqual(f.stats()['complete'],1)
        self.assertEqual(len(f.operations('claim')),1);self.assertEqual(len(f.operations('complete')),1)
        self.assertGreaterEqual(len(f.kernels()),3)
        spans=[]
        for p in f.kernels():
            rows=[r.split() for r in p.read_text().splitlines() if r.startswith('OPV1 ')]
            begin=rows[0];self.assertEqual(begin[:2],['OPV1','BEGIN'])
            progress=[int(r[2]) for r in rows if r[1]=='PROGRESS']
            self.assertTrue(progress);spans.append((int(begin[2],16),progress[-1]))
        cursor=s.BASE
        for start,n in sorted(spans):self.assertEqual(start,cursor);cursor+=n
        self.assertEqual(cursor,s.BASE+f.count)
        self.assertTrue(f.intent_checks)
        self.assertFalse((f.root/'receipt-client.json').stat().st_mode&0o077)
        self.assertFalse(any(r['path'].startswith('/api/range/') for r in f.requests))
    def test_normal_public_route_exact_native_coverage_and_private_intents(self):
        f=self.fixture();self.run_cli(f);self.complete(f);before=len(f.requests)
        self.run_cli(f,20,True);self.assertEqual(len(f.requests),before)
    def test_lost_claim_reply_resume_looks_up_same_operation_no_second_claim(self):
        f=self.fixture('lost-claim');self.run_cli(f,2);self.assertEqual(f.state()[0],'claiming');self.assertFalse(f.kernels())
        self.assertEqual(len(f.operations('claim')),1);self.assertFalse(f.queries())
        before=(f.root/'receipt-client.json').read_bytes();self.run_cli(f,20)
        self.assertEqual((f.root/'receipt-client.json').read_bytes(),before)
        self.run_cli(f,0,True);self.complete(f);self.assertEqual(len(f.queries()),1)
    def test_lost_progress_reply_pauses_then_reconciles_before_new_work(self):
        f=self.fixture('lost-progress');self.run_cli(f,3);self.assertEqual(f.state()[0],'paused')
        self.assertEqual(f.stats()['complete'],0);self.assertFalse(f.queries())
        self.run_cli(f,0,True);self.complete(f);self.assertEqual(len(f.queries()),1)
    def test_lost_completion_reply_resume_without_another_kernel_or_completion(self):
        f=self.fixture('lost-complete');self.run_cli(f,1);self.assertEqual(f.state()[0],'pending-complete')
        kernels={str(p):p.read_bytes() for p in f.kernels()};before=f.stats().copy();self.assertEqual(before['complete'],1)
        self.run_cli(f,0,True);self.complete(f)
        self.assertEqual(f.stats(),before);self.assertEqual(kernels,{str(p):p.read_bytes() for p in f.kernels()})
        self.assertEqual(len(f.queries()),1)
    def test_server_completion_receipt_cannot_replace_native_terminal_proof(self):
        f=self.fixture('lost-complete');self.run_cli(f,1)
        cp=sorted(f.root.glob('segment-*/checkpoint.txt'))[-1]
        raw=cp.read_text();self.assertIn('complete',raw);cp.write_text(raw.replace('complete','interrupted'))
        kernels={str(p):p.read_bytes() for p in f.kernels()};before=f.stats().copy()
        self.run_cli(f,2,True);self.assertEqual(f.state()[0],'pending-complete')
        self.assertEqual(f.stats(),before);self.assertEqual(kernels,{str(p):p.read_bytes() for p in f.kernels()})
    def test_peer_restart_queries_durable_receipt_and_unavailable_peer_preserves_intent(self):
        for kind,code in (('lost-claim',2),('lost-complete',1)):
            f=self.fixture(kind);self.run_cli(f,code);f.close()
            before=(f.root/'receipt-client.json').read_bytes();self.run_cli(f,2,True)
            self.assertEqual((f.root/'receipt-client.json').read_bytes(),before)
            db=f.ledger.path;database=db.read_bytes();kernels={str(p):p.read_bytes() for p in f.kernels()}
            peer=Fixture(f.root,db,target=f.target,port=f.server.server_port,client_id=f.client_id,resume_only=True)
            self.fixtures.append(peer);self.run_cli(peer,0,True)
            self.assertEqual(peer.state()[0],'done');self.assertEqual(peer.stats()['claim'],1);self.assertEqual(peer.stats()['complete'],1)
            self.assertEqual(len(peer.queries()),1);self.assertFalse(peer.operations('claim'))
            if kind=='lost-complete':
                self.assertFalse(peer.operations('complete'))
                self.assertEqual(database,db.read_bytes());self.assertEqual(kernels,{str(p):p.read_bytes() for p in f.kernels()})
            else:self.assertEqual(len(peer.operations('complete')),1)
    def test_unknown_claim_receipt_never_replays_or_launches(self):
        f=self.fixture('unknown-claim');self.run_cli(f,2);self.assertEqual(f.stats()['claim'],0)
        before=(f.root/'receipt-client.json').read_bytes();self.run_cli(f,2,True)
        self.assertEqual(before,(f.root/'receipt-client.json').read_bytes());self.assertEqual(len(f.operations('claim')),1)
        self.assertFalse(f.kernels());self.assertEqual(len(f.queries()),1)
    def test_mismatched_query_preserves_intent_and_blocks_kernel(self):
        f=self.fixture('lost-claim');self.run_cli(f,2);f.kind='query-mismatch'
        before=(f.root/'receipt-client.json').read_bytes();self.run_cli(f,2,True)
        self.assertEqual(before,(f.root/'receipt-client.json').read_bytes());self.assertFalse(f.kernels())
        self.assertEqual(len(f.operations('claim')),1)
    def test_changed_scope_and_boolean_epoch_response_cannot_launch(self):
        for kind in ('tamper-claim','wrong-types'):
            f=self.fixture(kind);self.run_cli(f,2);self.assertFalse(f.kernels());self.assertEqual(f.stats()['claim'],1)
            self.assertIsNone(f.journal()['entries']['0']['receipt']);f.kind='normal';self.run_cli(f,0,True);self.complete(f)
    def test_native_process_loss_at_claim_and_completion_boundaries(self):
        for kind in ('kill-claim','kill-complete'):
            f=self.fixture(kind);log=self.root/(kind+'.log')
            with log.open('w') as output:
                p=subprocess.Popen(f.command(),env=f.env(),stdout=output,stderr=output)
                try:
                    self.assertTrue(f.boundary.wait(12),log.read_text());self.assertIsNone(p.poll())
                    p.kill();p.wait(timeout=5)
                finally:
                    if p.poll() is None:p.kill();p.wait(timeout=5)
                    f.release.set()
            kernels={str(path):path.read_bytes() for path in f.kernels()}
            self.run_cli(f,0,True);self.complete(f)
            if kind=='kill-complete':self.assertEqual(kernels,{str(path):path.read_bytes() for path in f.kernels()})
    def test_changed_intent_digest_is_rejected_before_query(self):
        f=self.fixture('lost-claim');self.run_cli(f,2);path=f.root/'receipt-client.json';v=f.journal()
        v['entries']['0']['request_sha256']='0'*64;path.write_text(json.dumps(v));before=path.read_bytes()
        self.run_cli(f,2,True);self.assertEqual(path.read_bytes(),before);self.assertFalse(f.queries());self.assertFalse(f.kernels())
    def test_cached_receipt_is_rechecked_and_changed_assignment_preserved_for_review(self):
        import uuid
        f=self.fixture('lost-claim');self.run_cli(f,2);v=f.journal();row=v['entries']['0']
        row['receipt']=f.ledger.query(row['request'])['receipt']
        row['receipt']['assignment']['assignment_id']=str(uuid.uuid4())
        path=f.root/'receipt-client.json';path.write_text(json.dumps(v));before=path.read_bytes()
        self.run_cli(f,2,True);self.assertEqual(path.read_bytes(),before);self.assertEqual(len(f.queries()),1)
        self.assertFalse(f.kernels());self.assertEqual(len(f.operations('claim')),1)
    def test_missing_journal_and_changed_protocol_cannot_resume(self):
        f=self.fixture('lost-claim');self.run_cli(f,2)
        path=f.root/'receipt-client.json';archived=path.with_suffix('.preserved');path.rename(archived)
        self.run_cli(f,2,True);self.assertFalse(path.exists());self.assertFalse(f.queries())
        archived.rename(path);args=f.command(True);i=args.index('--receipt-protocol');del args[i:i+2]
        self.run_cli(f,2,args=args);self.assertFalse(f.queries());self.assertFalse(f.kernels())
    def test_lease_refused_before_kernel_remains_blocked(self):
        f=self.fixture('lease-before');self.run_cli(f,2);self.assertEqual(f.state()[0],'rejected')
        self.assertFalse(f.kernels());before=len(f.requests);self.run_cli(f,20,True);self.assertEqual(len(f.requests),before)
    def test_stop_resume_same_assignment_uses_new_lease_probe(self):
        f=self.fixture('cancel');self.run_cli(f,130);self.assertEqual(f.state()[0],'paused')
        old=f.stats()['progress'];f.kind='normal';self.run_cli(f,0,True)
        self.assertEqual(f.stats()['claim'],1);self.assertEqual(f.stats()['complete'],1)
        self.assertGreater(f.stats()['progress'],old);self.assertEqual(len(f.operations('claim')),1)
    def test_old_progress_receipt_does_not_authorize_kernel_after_lease_expiry(self):
        import time
        f=self.fixture('cancel');self.run_cli(f,130);kernels={str(p):p.read_bytes() for p in f.kernels()}
        f.kind='normal';f.ledger.clock=lambda:int(time.time())+400
        self.run_cli(f,2,True);self.assertEqual(f.state()[0],'rejected')
        self.assertEqual(kernels,{str(p):p.read_bytes() for p in f.kernels()});self.assertEqual(f.stats()['complete'],0)
        self.assertEqual(len(f.operations('claim')),1);self.assertEqual(len(f.queries()),1)
    def test_safe_stop_finishes_assignment_without_next_claim(self):
        f=self.fixture('safe');self.run_cli(f);self.complete(f)
    def test_solution_export_stays_private_and_never_completes(self):
        f=self.fixture(target=s.address(s.key_hash(s.BASE)));result=self.run_cli(f,10)
        self.assertEqual(f.state()[0],'solution');self.assertEqual(f.stats()['complete'],0)
        self.assertFalse(any('solution' in r['path'] for r in f.requests))
        self.assertNotIn('private_key_wif=',result.stdout+result.stderr)
        wallets=list((f.root/'home').rglob('wallet-import.txt'));self.assertEqual(len(wallets),1)
        self.assertFalse(wallets[0].stat().st_mode&0o077)
    def test_remote_endpoint_bad_version_and_missing_optin_have_no_effect(self):
        f=self.fixture()
        for key,value in (('--server','https://example.invalid'),('--receipt-protocol','other-version')):
            args=f.command();args[args.index(key)+1]=value;self.run_cli(f,2,args=args)
        args=f.command();args.remove('--enable-experimental-vanity-local');self.run_cli(f,2,args=args)
        self.assertFalse(f.requests);self.assertFalse(f.kernels());self.assertFalse((f.root/'coordinator-state.txt').exists())


if __name__=='__main__':unittest.main(verbosity=2)
