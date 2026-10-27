"""Synthetic durable server protocol; never invokes a native client or GPU."""
import concurrent.futures
import json
import os
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
import uuid
sys.dont_write_bytecode = True
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'measured'))
from ServerContract import Ledger, PROTOCOL, validate, scope_digest


SCOPE=dict(puzzle=20,range_id=37,start=f'{0x123456789000000000:064x}',end=f'{0x123456789000000000+127:064x}',target='1BoatSLRHtKNngkdXEeobR76b53LETtpyT',required_backend='cuda')

def request(kind='claim', **extra):
    return dict(protocol=PROTOCOL, operation_id=str(uuid.uuid4()), client_id='local-client-A',
                kind=kind, scope_sha256=scope_digest(SCOPE), **(dict(puzzle=20) if kind=='claim' else {}), **extra)


class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.root.chmod(0o700)
        self.path=self.root/'ledger.sqlite';self.now=1000
        self.ledger=Ledger(self.path,total=128,scope=SCOPE,clock=lambda:self.now)

    def tearDown(self):self.tmp.cleanup()

    def assigned(self):return self.ledger.execute(request())['assignment']

    def operation(self, kind, a, n):
        return request(kind,assignment_id=a['assignment_id'],lease_epoch=a['lease_epoch'],keys_checked=str(n))

    def test_lost_claim_reply_reopen_replays_identical_receipt(self):
        q=request();first=self.ledger.execute(q)
        other=Ledger(self.path,total=128,scope=SCOPE)
        self.assertEqual(other.execute(q),first)
        self.assertEqual(other.query(q)['receipt'],first)
        self.assertEqual(other.execute(request())['outcome'],'refused')

    def test_complete_reply_loss_same_operation_after_expiry(self):
        a=self.assigned();self.ledger.execute(self.operation('progress',a,128))
        q=self.operation('complete',a,128);first=self.ledger.execute(q);self.now=2000
        self.assertEqual(self.ledger.execute(q),first)
        self.assertEqual(first['assignment']['status'],'completed')
        self.assertEqual(self.ledger.execute(self.operation('complete',a,128))['outcome'],'refused')
        self.assertEqual(self.ledger.execute(request())['outcome'],'refused')

    def test_different_payload_cannot_reuse_operation_id(self):
        q=request();self.ledger.execute(q);q['client_id']='local-client-B'
        with self.assertRaisesRegex(RuntimeError,'conflict'):self.ledger.execute(q)
        with self.assertRaisesRegex(RuntimeError,'conflict'):self.ledger.query(q)

    def test_concurrent_duplicate_claim_has_one_effect(self):
        q=request()
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results=list(pool.map(lambda _:self.ledger.execute(q),range(16)))
        self.assertTrue(all(r==results[0] for r in results))
        with sqlite3.connect(self.path) as db:
            self.assertEqual(db.execute('SELECT count(*) FROM assignment').fetchone()[0],1)
            self.assertEqual(db.execute('SELECT count(*) FROM receipts').fetchone()[0],1)

    def test_concurrent_distinct_claims_do_not_allocate_twice(self):
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results=list(pool.map(lambda _:self.ledger.execute(request()),range(16)))
        self.assertEqual(sum(r['outcome']=='applied' for r in results),1)

    def test_effect_and_receipt_roll_back_together(self):
        q=request()
        def fail():raise RuntimeError('synthetic transaction fault')
        with self.assertRaisesRegex(RuntimeError,'transaction fault'):self.ledger.execute(q,before_commit=fail)
        self.assertEqual(self.ledger.query(q)['state'],'unknown')
        self.assertEqual(self.ledger.execute(q)['outcome'],'applied')

    def test_completion_needs_previously_recorded_full_progress(self):
        a=self.assigned()
        self.assertEqual(self.ledger.execute(self.operation('complete',a,128))['reason'],'coverage_refused')
        self.ledger.execute(self.operation('progress',a,128))
        self.assertEqual(self.ledger.execute(self.operation('complete',a,127))['outcome'],'refused')

    def test_regression_overflow_and_expired_lease_refused(self):
        a=self.assigned();self.ledger.execute(self.operation('progress',a,64))
        for n in (63,129):
            self.assertEqual(self.ledger.execute(self.operation('progress',a,n))['reason'],'coverage_refused')
        self.now=1300
        self.assertEqual(self.ledger.execute(self.operation('progress',a,64))['reason'],'lease_refused')

    def test_client_and_epoch_fencing(self):
        a=self.assigned();q=self.operation('progress',a,0);q['lease_epoch']=2
        self.assertEqual(self.ledger.execute(q)['reason'],'lease_refused')
        q=self.operation('progress',a,0);q['client_id']='local-client-B'
        r=self.ledger.execute(q)
        self.assertEqual(r['reason'],'assignment_not_found');self.assertIsNone(r['assignment'])

    def test_query_changes_no_database_bytes(self):
        q=request();self.ledger.execute(q);before=self.path.read_bytes()
        self.ledger.query(q);self.ledger.query(request())
        self.assertEqual(self.path.read_bytes(),before)
        self.assertFalse((self.root/'ledger.sqlite-journal').exists())

    def test_strict_counts_and_request_fields(self):
        a=self.assigned()
        for n in ('01','-1','1.0','1e3',True,1,'18446744073709551616','١'):
            q=self.operation('progress',a,0);q['keys_checked']=n
            with self.assertRaises((RuntimeError,ValueError)):self.ledger.execute(q)
        q=request();q['secret']='not-supported'
        with self.assertRaises(RuntimeError):validate(q)
        q=request();q['puzzle']=True
        with self.assertRaises(RuntimeError):validate(q)

    def test_binding_mismatch_preserves_database(self):
        before=self.path.read_bytes()
        with self.assertRaises(RuntimeError):Ledger(self.path,total=127,scope=SCOPE)
        self.assertEqual(before,self.path.read_bytes())

    def test_private_file_and_symlink_refused(self):
        self.path.chmod(0o644)
        with self.assertRaises(RuntimeError):Ledger(self.path,total=128,scope=SCOPE)
        alias=self.root/'alias';alias.symlink_to(self.path)
        with self.assertRaises(RuntimeError):Ledger(alias,total=128,scope=SCOPE)


if __name__=='__main__':unittest.main(verbosity=2)
