"""Lost replies and actual isolated server process loss; no native/GPU execution."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent))
from ReceiptLedgerTests import request, SCOPE
from ServerContract import Ledger, reconcile


class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.root.chmod(0o700)
        self.path=self.root/'ledger.sqlite';self.ledger=Ledger(self.path,total=128,scope=SCOPE,clock=lambda:1000)

    def tearDown(self):self.tmp.cleanup()

    def operation(self,kind,a,n):
        return request(kind,assignment_id=a['assignment_id'],lease_epoch=a['lease_epoch'],keys_checked=str(n))

    def child(self,q,stage):
        module=str(Path(__file__).resolve().parents[1]/'measured')
        code='''import json,sys,time
sys.path.insert(0,sys.argv[1])
from ServerContract import Ledger
SCOPE=json.loads(sys.argv[5])
q=json.loads(sys.argv[3]);stage=sys.argv[4]
ledger=Ledger(sys.argv[2],total=128,scope=SCOPE,clock=lambda:1000)
def pause():
 print('BOUNDARY',flush=True)
 time.sleep(30)
r=ledger.execute(q,before_commit=pause if stage=='before' else None)
if stage=='after':pause()
'''
        p=subprocess.Popen([sys.executable,'-B','-u','-c',code,module,str(self.path),json.dumps(q),stage,json.dumps(SCOPE)],
            stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            # select bounds even a failed child; no unbounded readline on a live process.
            import selectors
            with selectors.DefaultSelector() as ready:
                ready.register(p.stdout,selectors.EVENT_READ)
                self.assertTrue(ready.select(10),'No transaction boundary')
                self.assertEqual(p.stdout.readline().strip(),'BOUNDARY')
            p.kill();p.wait(timeout=10)
        finally:
            if p.poll() is None:p.kill();p.wait(timeout=10)
            p.stdout.close();p.stderr.close()

    def test_process_killed_before_commit_has_no_claim_or_receipt(self):
        q=request();self.child(q,'before')
        other=Ledger(self.path,total=128,scope=SCOPE,clock=lambda:1000)
        self.assertEqual(reconcile(q,other.query(q)),'review_unknown')
        self.assertEqual(other.execute(q)['outcome'],'applied')

    def test_process_killed_after_commit_lost_claim_reply_recovered(self):
        q=request();self.child(q,'after')
        other=Ledger(self.path,total=128,scope=SCOPE)
        r=other.query(q)
        self.assertEqual(reconcile(q,r),'claim_recorded')
        self.assertEqual(other.execute(q),r['receipt'])

    def test_completion_committed_then_server_killed_no_second_effect(self):
        a=self.ledger.execute(request())['assignment'];self.ledger.execute(self.operation('progress',a,128))
        q=self.operation('complete',a,128);self.child(q,'after')
        other=Ledger(self.path,total=128,scope=SCOPE)
        self.assertEqual(reconcile(q,other.query(q)),'completion_recorded')
        self.assertEqual(other.execute(q)['assignment']['status'],'completed')
        self.assertEqual(other.execute(request())['outcome'],'refused')

    def test_completion_interrupted_before_commit_preserves_assigned_state(self):
        a=self.ledger.execute(request())['assignment'];self.ledger.execute(self.operation('progress',a,128))
        q=self.operation('complete',a,128);self.child(q,'before')
        other=Ledger(self.path,total=128,scope=SCOPE,clock=lambda:1000)
        self.assertEqual(reconcile(q,other.query(q)),'review_unknown')
        with other.connect(readonly=True) as db:
            self.assertEqual(db.execute('SELECT status,checked FROM assignment').fetchone(),('assigned','128'))
        self.assertEqual(other.execute(q)['outcome'],'applied')

    def test_progress_receipt_is_operation_snapshot(self):
        a=self.ledger.execute(request())['assignment'];q=self.operation('progress',a,64)
        self.ledger.execute(q);self.ledger.execute(self.operation('progress',a,128))
        self.assertEqual(reconcile(q,self.ledger.query(q)),'progress_recorded')
        self.assertEqual(self.ledger.query(q)['receipt']['assignment']['keys_checked'],'64')

    def test_wrong_query_identity_digest_and_protocol_blocked(self):
        q=request();self.ledger.execute(q);response=self.ledger.query(q)
        for key,value in [('client_id','other'),('request_sha256','0'*64),('protocol','other'),
                          ('operation_id',request()['operation_id']),('production_enabled',True)]:
            bad=copy.deepcopy(response);bad[key]=value
            with self.assertRaises(RuntimeError):reconcile(q,bad)

    def test_tampered_receipt_never_confirms_completion(self):
        a=self.ledger.execute(request())['assignment'];self.ledger.execute(self.operation('progress',a,128))
        q=self.operation('complete',a,128);self.ledger.execute(q);response=self.ledger.query(q)
        for key,value in [('keys_checked','127'),('total','129'),('status','assigned'),('lease_epoch',2),
                          ('assignment_id',request()['operation_id']),('client_id','other')]:
            bad=copy.deepcopy(response);bad['receipt']['assignment'][key]=value
            with self.assertRaises(RuntimeError):reconcile(q,bad)
        for key,value in [('target','1BitcoinEaterAddressDontSendf59kuE'),('start','0'*64),('required_backend','opencl')]:
            bad=copy.deepcopy(response);bad['receipt']['assignment']['scope'][key]=value
            with self.assertRaises(RuntimeError):reconcile(q,bad)

    def test_refused_lease_receipt_does_not_authorize_work(self):
        a=self.ledger.execute(request())['assignment'];q=self.operation('progress',a,0);q['lease_epoch']=2
        self.ledger.execute(q)
        self.assertEqual(reconcile(q,self.ledger.query(q)),'blocked_refused')

    def test_unknown_does_not_infer_an_effect_or_retry(self):
        q=request();response=self.ledger.query(q)
        self.assertEqual(reconcile(q,response),'review_unknown')
        response['receipt']={}
        with self.assertRaises(RuntimeError):reconcile(q,response)


if __name__=='__main__':unittest.main(verbosity=2)
