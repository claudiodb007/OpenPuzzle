"""GPL-3.0-only. Read-only receipt/ledger evidence for explicit desktop recovery.

No socket, mutation, process launch, checkpoint edit or transaction repair.
Unknown results and SQLite recovery sidecars require review.
"""
import hashlib
import json
import re
from contextlib import closing
import Evidence as e
import Resume as r
import Support as s
from ServerContract import Ledger, PROTOCOL, encoded, validate, reconcile, scope_digest


def read(path):
    def pairs(items):
        value={}
        for key,item in items:
            e.require(key not in value,'Duplicate receipt evidence JSON key');value[key]=item
        return value
    def constant(_):raise RuntimeError('Nonfinite receipt evidence value')
    e.require(path.stat().st_size<=4*1024*1024,'Oversized receipt evidence')
    return json.loads(path.read_text(),object_pairs_hook=pairs,parse_constant=constant)


def scope(meta):
    return dict(puzzle=20,range_id=37,start=f'{s.BASE:064x}',end=f'{s.BASE+2**36-1:064x}',
                target=meta['target'],required_backend='cuda')


def ledger(directory,meta):
    path=r.private_file(directory,'peer/ledger.sqlite')
    e.require(not any(path.with_name(path.name+suffix).exists() or path.with_name(path.name+suffix).is_symlink()
                      for suffix in ('-journal','-wal','-shm')),'Ledger transaction requires review; no automatic repair')
    value=Ledger(path,total=2**36,scope=scope(meta))  # Existing-file path only, read-only metadata check.
    with closing(value.connect(readonly=True)) as db:
        e.require(db.execute('PRAGMA journal_mode').fetchone()==('delete',),'Unsupported receipt ledger journal mode')
    return value


def history(directory,meta):
    value=read(r.private_file(directory,'native/receipt-client.json'))
    e.require(set(value)=={'schema','protocol','binding','server','client_id','scope','entries'}
              and value['schema']=='openpuzzle_native_receipts_v1' and value['protocol']==PROTOCOL
              and value['server']==meta['native_options']['--server'] and value['client_id']==meta['client_id']
              and value['scope']==scope(meta),'Native receipt journal binding differs')
    raw=r.private_file(directory,'native/session-binding.txt').read_bytes()
    e.require(value['binding']==hashlib.sha256(raw).hexdigest(),'Native receipt session fingerprint differs')
    rows=value['entries']
    e.require(type(rows) is dict and 0<len(rows)<=4096 and set(rows)=={str(i) for i in range(len(rows))},'Missing/gapped receipt operation history')
    peer=ledger(directory,meta);queries={};assignment=None;checked=0;completions=0;refused=False;unconfirmed=False
    for i in range(len(rows)):
        row=rows[str(i)]
        e.require(type(row) is dict and set(row)=={'request','request_sha256','receipt'},'Native receipt entry differs')
        q=row['request'];digest=validate(q)
        e.require(digest==row['request_sha256'] and q['client_id']==meta['client_id']
                  and q['scope_sha256']==scope_digest(scope(meta)) and q['operation_id'] not in queries,
                  'Native operation identity/digest/scope differs')
        e.require(not completions and not refused,'Operation follows terminal/refused receipt')
        kind=q['kind']
        if i==0:e.require(kind=='claim','First operation is not a claim')
        else:
            e.require(kind in ('progress','complete') and q['assignment_id']==assignment['assignment_id']
                      and q['lease_epoch']==assignment['lease_epoch'],'Receipt history assignment changed/new claim')
            n=int(q['keys_checked']);e.require(checked<=n<=2**36,'Receipt count regressed/escaped')
        response=peer.query(q);decision=reconcile(q,response)
        e.require(decision!='review_unknown','Receipt unknown; review required, no replay')
        actual=response['receipt'];cached=row['receipt']
        e.require(cached is None and i==len(rows)-1 or cached==actual,'Native cached receipt differs from durable peer')
        unconfirmed=cached is None
        if decision=='blocked_refused':
            e.require(i==len(rows)-1,'Operation follows refused receipt');refused=True
        elif kind=='claim':assignment=actual['assignment']
        elif kind=='progress':checked=int(q['keys_checked'])
        else:
            e.require(checked==int(q['keys_checked'])==2**36,'Completion lacks prior full progress');completions+=1
        queries[q['operation_id']]=response
    e.require(assignment is not None,'Claim was not durably accepted')
    with closing(peer.connect(readonly=True)) as db:
        stored=db.execute('SELECT operation_id,client,digest,receipt FROM receipts').fetchall()
        e.require(len(stored)==len(queries),'Peer has missing/extra operations')
        for op,client,digest,receipt in stored:
            e.require(op in queries and client==meta['client_id'] and digest==queries[op]['request_sha256']
                      and json.loads(receipt)==queries[op]['receipt'],'Peer receipt row differs')
        current=db.execute('SELECT id,client,epoch,lease_until,checked,status FROM assignment').fetchall()
    expected=(assignment['assignment_id'],meta['client_id'],assignment['lease_epoch'],assignment['lease_until'],
              str(checked),'completed' if completions else 'assigned')
    e.require(current==[expected],'Current peer assignment differs from exact receipt history')
    return dict(assignment=assignment,acknowledged=checked,completions=completions,refused=refused,
                unconfirmed=unconfirmed,queries=queries,last=rows[str(len(rows)-1)]['request'])


def live(directory,meta,owner_token,journal_sha256=None):
    path=r.private_file(directory,'http-live.json')
    if journal_sha256 is not None:e.require(s.sha(path)==journal_sha256,'Live receipt HTTP journal changed')
    value=read(path)
    expected=dict(metadata_sha256=s.sha(r.private_file(directory,'coordinator-metadata.json')),
                  client_id=meta['client_id'],server=meta['native_options']['--server'])
    e.require(set(value)=={'schema','identity','owner_token','requests','responses','pending'}
              and value['schema']=='openpuzzle_http_live_v1' and value['identity']==expected
              and value['owner_token']==owner_token,'Receipt live HTTP identity differs')
    calls,replies=value['requests'],value['responses']
    e.require(type(calls) is list and type(replies) is list and len(calls)<=8192,'Invalid receipt HTTP history')
    pending=value['pending']
    e.require((pending is None and len(calls)==len(replies))
              or (len(calls)==len(replies)+1 and pending==calls[-1]
                  and pending['path'] in ('/api/vanity/v1/operations','/api/vanity/v1/receipts/query')),
              'Unreviewed incomplete receipt HTTP request')
    return value


def http_state(directory,live,meta,state):
    queries=state['queries'];executed=set();registrations=0
    calls,replies=live['requests'],live['responses']
    for i,call in enumerate(calls):
        e.require(type(call) is dict and set(call)=={'path','payload'},'HTTP call shape differs')
        path,body=call['path'],call['payload'];e.require(body.get('client_id')==meta['client_id'],'HTTP client differs')
        event=replies[i] if i<len(replies) else None
        if event is not None:
            e.require(set(event)=={'path','payload','status','reply','applied'} and event['path']==path and event['payload']==body
                      and type(event['status']) is int and 100<=event['status']<=599
                      and event['applied']==dict(claim=False,progress=False,complete=False,solution=False)
                      and all(type(v) is bool for v in event['applied'].values()),'Receipt HTTP event differs')
        if path in ('/api/vanity/v1/operations','/api/vanity/v1/receipts/query'):
            digest=validate(body);op=body['operation_id']
            e.require(op in queries and digest==queries[op]['request_sha256'],'HTTP operation is not bound to native intent')
            if path.endswith('/operations'):
                e.require(op not in executed,'Operation was replayed to peer');executed.add(op)
            if event and event['status']==200:
                expected=queries[op] if path.endswith('/query') else queries[op]['receipt']
                e.require(event['reply']==expected,'HTTP reply differs from durable receipt')
        elif path=='/api/client/register':
            registrations+=1;e.require(body==dict(client_id=meta['client_id']),'Registration differs')
        elif path=='/api/client/heartbeat':
            e.require(body.get('engines')==[dict(name='Vanity',backend='cuda',installed=True,available=True)],'Heartbeat differs')
        else:raise RuntimeError('Unsupported receipt desktop endpoint')
        if event:
            code=event['status']
            if code==599 and type(event['reply']) is dict and event['reply'].get('error')=='receipt_resolved_without_delivery':
                proof=event['reply'];e.require(set(proof)=={'error','archive','sha256','request_sha256'},'HTTP resolution shape differs')
                e.require(isinstance(proof['archive'],str) and proof['archive'].startswith('coordinator-history/'),'HTTP resolution archive differs')
                source=r.private_file(directory,proof['archive']);e.require(s.sha(source)==proof['sha256'],'HTTP resolution archive changed')
                previous=read(source)
                e.require(previous['pending']==call and previous['requests']==calls[:i+1]
                          and previous['responses']==replies[:i] and proof['request_sha256']==validate(body),
                          'HTTP resolution does not match original undelivered request')
            if path.startswith('/api/client/') and code==200:e.require(event['reply']==dict(success=True),'Client acknowledgement differs')
            e.require(code==200 or code in (408,429) or 500<=code<=599,'Permanent HTTP response requires review')
    e.require(registrations==1 and executed==set(queries),'Missing/extra registration or native operation delivery')


def outcome(directory,native,token,exit_code=None):
    raw=r.private_file(directory,'native/coordinator-state.txt').read_text().splitlines()
    phase=raw[2].split()[0]
    if phase=='claiming':
        e.require(exit_code in (None,2),'Unexpected unconfirmed claim exit');return dict(kind='receipt-pending',operation='claim')
    rows=r.private_file(directory,'native/coordinator-outcome.txt').read_text().splitlines()
    binding=hashlib.sha256(r.private_file(directory,'native/session-binding.txt').read_bytes()).hexdigest()
    e.require(len(rows)==3 and rows[:2]==['OPV-RECOVERY-1',binding],'Native receipt outcome binding differs')
    fields=rows[2].split();e.require(len(fields)==10,'Native receipt outcome shape differs')
    kind,operation,status,code,p,offset,checked,launches,pid,attempt=fields
    e.require(attempt==token,'Stale native invocation outcome')
    e.require(' '.join((p,offset,checked,launches))==raw[2] and p==native['phase']
              and int(checked)==native['checked'] and int(launches)==native['launches'] and int(pid)>1,'Stale native receipt counters')
    code,status=int(code),int(status)
    if exit_code is not None:e.require(code==exit_code,'Native exit and outcome differ')
    transient=status in (0,408,429) or 500<=status<=599
    valid=(p=='done' and kind=='done' and operation=='complete' and code==0 and status==200
           or p=='paused' and kind=='paused' and operation=='control' and code==130
           or p=='paused' and kind=='network' and operation in ('progress','heartbeat') and code in (2,3) and transient
           or p=='pending-complete' and kind=='network' and operation=='complete' and code==1 and transient
           or p=='rejected' and kind=='lease' and operation in ('progress','heartbeat','complete') and code in (1,2) and 400<=status<=599)
    e.require(valid,'Unreviewed native receipt outcome')
    return dict(kind=kind,operation=operation,http_status=status,exit_code=code,owner_pid=int(pid),owner_token=attempt)
