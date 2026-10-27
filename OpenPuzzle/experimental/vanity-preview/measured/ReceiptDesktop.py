"""GPL-3.0-only. Receipt-backed owned desktop sessions, local opt-in store only.

Existing UI commands/controls are reused. Lists and snapshot proposals are
read-only; a confirmed Resume owns every mutation. No production fallback.
"""
import hashlib
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import threading
import time
import uuid
import Coordinator as c
import Evidence as e
import Profile as p
import Resume as r
import Support as s
import OwnerRecovery as owner
import ReceiptEvidence as proof
import ReceiptCoordinator as peer
import ServerContract as contract

MODE='coordinator-receipts.json'
SCHEMA='openpuzzle_receipt_desktop_v1'
ACTIVE_SCHEMA='openpuzzle_receipt_owned_v1'
SEAL_SCHEMA='openpuzzle_receipt_seal_v1'
ORDER=('--server','--root','--controls','--engine','--engine-sha256','--adapter','--adapter-sha256',
       '--client-id','--target','--puzzle','--grid','--segment-keys','--scope-start','--scope-end',
       '--gpu-uuid','--speed-mkeys','--receipt-protocol')


def sources():
    # Include transitive helpers (measurement, loopback HTTP and recovery),
    # not only the modules directly imported by this dispatcher.
    return {path.name:s.sha(path) for path in sorted(Path(__file__).parent.glob('*.py'))}


def mode_value(store,cli):
    record,_,_=p.read_selected(store)
    return dict(schema='openpuzzle_receipt_store_v1',protocol=contract.PROTOCOL,profile_binding=record['binding_sha256'],
                cli_sha256=s.sha(cli),sources=sources(),production_enabled=False)


def enable_store(store,cli):
    store=p.private_directory(store);value=mode_value(store,cli);path=store/MODE
    if path.exists() or path.is_symlink():
        stored=proof.read(r.private_file(store,MODE));e.require(stored==value and stored['production_enabled'] is False,'Receipt store configuration differs')
    else:r.atomic(path,value)


def enabled(store,cli):
    path=store/MODE
    if not path.exists() and not path.is_symlink():
        e.require(not any(is_receipt(d) for d in store.glob('run-*')),'Receipt store configuration missing')
        return False
    value=proof.read(r.private_file(store,MODE))
    e.require(value==mode_value(store,cli) and value['production_enabled'] is False,'Receipt store configuration changed')
    return True


def is_receipt(directory):
    try:return proof.read(r.private_file(Path(directory),c.META)).get('schema')==SCHEMA
    except Exception:return False  # Legacy inspector still refuses unknown/damaged metadata.


def expected_options(directory,meta,record,paths):
    native=directory/'native'
    return dict(zip(ORDER,(meta['native_options']['--server'],str(native),str(native/'controls'),str(paths['engine']),
        s.sha(paths['engine']),str(paths['adapter']),s.sha(paths['adapter']),meta['client_id'],meta['target'],'20',
        str(record['grid']),str(c.SEGMENT),f'{s.BASE:064x}',f'{s.BASE+c.TOTAL-1:064x}',record['uuid'],
        format(record['planning_mkeys'],'.12f'),contract.PROTOCOL)))


def command(cli,options,resume=False):
    result=[str(cli),'run','20','--engine','vanity','--backend','cuda',
            '--vanity-native-local','--enable-experimental-vanity-local']
    for key in ORDER:
        if key!='--puzzle':result.extend(['--target-engine' if key=='--engine' else key,options[key]])
    return result+(['--resume'] if resume else [])


def bound(store,directory,cli):
    r.owned(store,directory);e.require(enabled(store,cli),'Receipt-enabled store required')
    meta=proof.read(r.private_file(directory,c.META));record,_,paths=p.read_selected(store)
    e.require(set(meta)=={'schema','cli','cli_sha256','profile_binding','gpu_uuid','grid','planning_mkeys',
        'target','client_id','native_options','argv','configuration_sha256','sources'} and meta['schema']==SCHEMA,
        'Receipt desktop metadata differs')
    e.require(meta['cli']==str(cli) and meta['cli_sha256']==s.sha(cli) and meta['sources']==sources()
              and meta['configuration_sha256']==s.sha(r.private_file(store,MODE)),'Receipt desktop source/configuration differs')
    e.require(meta['profile_binding']==record['binding_sha256'] and meta['gpu_uuid']==record['uuid']
              and type(meta['grid']) is int and meta['grid']==record['grid']
              and meta['planning_mkeys']==record['planning_mkeys'] and meta['target']==s.address(s.key_hash(s.BASE-1))
              and re.fullmatch(r'opv-coord-[A-Za-z0-9-]{1,80}',meta['client_id']),'Receipt measured profile/target differs')
    url=meta['native_options']['--server'];e.require(re.fullmatch(r'http://127\.0\.0\.1:[1-9][0-9]{3,4}',url)
        and 1024<=int(url.rsplit(':',1)[1])<=65535,'Nonlocal receipt endpoint')
    opts=expected_options(directory,meta,record,paths)
    e.require(meta['native_options']==opts and meta['argv']==command(cli,opts),'Receipt native argument binding differs')
    return meta,record,paths


def idle(directory):
    c.native_idle(directory)
    import fcntl
    for path in (directory/'native').glob('segment-*/owner.lock'):
        with r.private_file(directory,str(path.relative_to(directory))).open('rb') as handle:
            fcntl.flock(handle,fcntl.LOCK_EX|fcntl.LOCK_NB)


def checked_evidence(directory,meta,live,cold=False,token=None,code=None):
    ledger=proof.history(directory,meta)
    native=c.coverage(directory,meta,cold=True,assignment_id=ledger['assignment']['assignment_id'],
                      journal=dict(completions=ledger['completions']))
    e.require(ledger['acknowledged']<=native['checked'],'Peer acknowledged more than native durable coverage')
    proof.http_state(directory,live,meta,ledger)
    raw=r.private_file(directory,'native/coordinator-state.txt').read_text().splitlines()[2].split()
    e.require(native['phase']!='done' or ledger['completions']==1 and not ledger['unconfirmed'],'Native done lacks confirmed completion')
    e.require(not ledger['completions'] or native['checked']==c.TOTAL,'Server completion lacks native coverage')
    if cold and native['phase']!='done':
        outcome=directory/'native/coordinator-outcome.txt'
        if outcome.exists():
            fields=r.private_file(directory,'native/coordinator-outcome.txt').read_text().splitlines()[2].split()
            if len(fields)==10 and fields[-1]==token:e.require(fields[0] not in ('lease','review'),'Native refusal blocks owner recovery')
        recovery=dict(kind='receipt-owner-loss',operation=ledger['last']['kind'])
    else:
        recovery=proof.outcome(directory,native,token,code)
        if ledger['unconfirmed'] and recovery['kind']=='network':recovery['kind']='receipt-pending'
    e.require(not ledger['refused'] or raw[0]=='rejected','Refused receipt requires explicit review')
    recovery.update(acknowledged=ledger['acknowledged'],completions=ledger['completions'])
    return native,recovery,ledger


def snapshot(store,directory,cli):
    directory=r.owned(store,directory);meta,record,paths=bound(store,directory,cli)
    active=(directory/c.ACTIVE).exists() or (directory/c.ACTIVE).is_symlink()
    if active:
        transaction=proof.read(r.private_file(directory,c.ACTIVE))
        e.require(transaction.get('schema')==ACTIVE_SCHEMA and transaction.get('review_required') is False,'Receipt transaction requires review')
        e.require(owner.dead(transaction['owner']) and owner.dead(transaction['native']),'Previous receipt owner is still active or unknown')
        idle(directory)
        token=transaction['owner_token'];live=proof.live(directory,meta,token,transaction['journal_sha256'])
    else:
        idle(directory)
        seal=proof.read(r.private_file(directory,c.SEAL))
        e.require(seal['schema']==SEAL_SCHEMA and seal['files']==c.pins(directory),'Receipt sealed evidence changed')
        token=seal['owner_token'];live=proof.live(directory,meta,token)
        e.require(live['pending'] is None,'Sealed receipt HTTP history is incomplete')
        e.require(proof.read(r.private_file(directory,'http-requests.json'))==live['requests']
                  and proof.read(r.private_file(directory,'http-responses.json'))==live['responses'],'Published receipt HTTP history differs')
    native,recovery,ledger=checked_evidence(directory,meta,live,cold=active,token=token)
    if not active:e.require(seal['native']==native and seal['recovery']==recovery,'Receipt seal disposition differs')
    eligible=native['phase'] in ('claiming','paused','pending-complete') or active and native['phase']=='done'
    reason='Completed receipt run' if native['phase']=='done' else 'Native receipt state requires review'
    if ledger['refused'] or native['phase']=='rejected':eligible=False;reason='Lease/assignment receipt refused'
    if not ledger['completions'] and ledger['assignment']['lease_until']<=int(time.time()):eligible=False;reason='Local reference lease expired; recovery blocked'
    value=dict(schema='openpuzzle_receipt_snapshot_v1',native=native,recovery=recovery,files=c.pins(directory),
        active_sha256=s.sha(directory/c.ACTIVE) if active else None,seal_sha256=s.sha(directory/c.SEAL) if (directory/c.SEAL).exists() else None,
        configuration_sha256=s.sha(store/MODE),available=eligible,reason=reason if not eligible else '',cold=active)
    value['state_sha256']=owner.token(value)
    return value,meta,record,paths,live,ledger


def row(store,directory,cli):
    value=dict(path=str(directory),available=False)
    try:
        state,*_=snapshot(store,directory,cli)
        value.update(available=state['available'],checked=state['native']['checked'],total=c.TOTAL,
            phase=state['native']['phase'],recovery=state['recovery']['kind'],state_sha256=state['state_sha256'])
        if not state['available']:value['reason']=state['reason']
    except Exception as error:value['reason']='Receipt recovery requires review: '+str(error)
    return value


def archive(directory,state):
    history=directory/'coordinator-history'/str(uuid.uuid4());history.mkdir(parents=True,mode=0o700)
    names=list(state['files'])+([c.ACTIVE] if state['active_sha256'] else [])+([c.SEAL] if state['seal_sha256'] else [])
    for name in names:
        src=r.private_file(directory,name);expected=state['files'].get(name,state['active_sha256'] if name==c.ACTIVE else state['seal_sha256'])
        e.require(s.sha(src)==expected,'Receipt evidence changed before archive')
        target=history/'before'/name;target.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
        shutil.copyfile(src,target);target.chmod(0o600);e.require(s.sha(target)==expected,'Receipt archive differs')
    r.atomic(history/'snapshot.json',state)
    return history


def publish(directory,live):
    r.atomic(directory/'http-requests.json',live['requests'],replace=(directory/'http-requests.json').exists())
    r.atomic(directory/'http-responses.json',live['responses'],replace=(directory/'http-responses.json').exists())


def seal(directory,native,recovery,token):
    r.atomic(directory/c.SEAL,dict(schema=SEAL_SCHEMA,native=native,recovery=recovery,owner_token=token,files=c.pins(directory)),
             replace=(directory/c.SEAL).exists())


def execute(store,directory,cli,expected=None):
    store=p.private_directory(store);e.require(enabled(store,cli),'Receipt-enabled store required')
    resume=expected is not None;state=meta=live=None;history=None;process=None;active=False;handlers={};cancel=False
    record,_,paths=p.read_selected(store)
    if resume:
        state,meta,record,paths,live,ledger=snapshot(store,directory,cli)
        e.require(state['state_sha256']==expected,'Stale receipt snapshot confirmation')
        e.require(state['available'],state['reason'])
    else:
        e.require(directory.is_absolute() and directory.parent==store and directory.resolve()==directory
                  and re.fullmatch(r'run-[0-9a-f-]{36}',directory.name) and not directory.exists(),'Fresh owned receipt directory required')
    if resume and state['cold'] and state['native']['phase']=='done':
        e.require(snapshot(store,directory,cli)[0]==state,'Receipt evidence changed before adoption')
        e.require(live['pending'] is None,'Completed receipt HTTP evidence requires review')
        archive(directory,state);publish(directory,live)
        # A final seal uses the exact durable native done outcome, not the cold label.
        final,disposition,_=checked_evidence(directory,meta,live,token=live['owner_token'])
        seal(directory,final,disposition,live['owner_token']);(directory/c.ACTIVE).unlink()
        c.emit(status='completed',checked=c.TOTAL,total=c.TOTAL,report=str(directory),next_claim_blocked=True,production_enabled=False);return 0
    terminal_only=resume and state['native']['checked']==c.TOTAL
    if terminal_only:gpu=record['uuid'];limit=None
    else:gpu,limit=p.hardware(record)
    if not resume:
        directory.mkdir(mode=0o700);(directory/'native').mkdir();(directory/'native/home').mkdir();(directory/'peer').mkdir()
    native=directory/'native';port=int(meta['native_options']['--server'].rsplit(':',1)[1]) if resume else 0
    f=peer.ReceiptAssignmentServer(native,ledger_path=directory/'peer/ledger.sqlite',cli=cli,adapter=paths['adapter'],
        engine=paths['engine'],count=c.TOTAL,grid=record['grid'],segment=c.SEGMENT,target=s.address(s.key_hash(s.BASE-1)),
        planning=record['planning_mkeys'],gpu_uuid=gpu,port=port,client_id=meta['client_id'] if resume else None,resume_only=resume)
    initial=state['native']['checked'] if resume else 0;guard=threading.RLock();transaction=None
    try:
        if resume:
            e.require(snapshot(store,directory,cli)[0]==state,'Receipt evidence changed before resume')
            e.require(f.command()==meta['argv'],'Receipt resume arguments differ');history=archive(directory,state)
        else:
            meta=dict(schema=SCHEMA,cli=str(cli),cli_sha256=s.sha(cli),profile_binding=record['binding_sha256'],
                gpu_uuid=gpu,grid=record['grid'],planning_mkeys=record['planning_mkeys'],target=f.target,client_id=f.client_id,
                native_options=f.options(),argv=f.command(),configuration_sha256=s.sha(store/MODE),sources=sources())
            r.atomic(directory/c.META,meta)
        token=str(uuid.uuid4());transaction=dict(schema=ACTIVE_SCHEMA,owner=owner.capture(os.getpid()),native=None,
            owner_token=token,review_required=False,resume=resume,expected=expected,history=str(history) if history else None)
        r.atomic(directory/c.ACTIVE,transaction,replace=bool(resume and state['cold']));active=True
        def published(digest):
            with guard:transaction['journal_sha256']=digest;r.atomic(directory/c.ACTIVE,transaction,replace=True)
        calls=list(live['requests']) if resume else [];responses=list(live['responses']) if resume else []
        if resume and live['pending'] is not None:
            old=history/'before/http-live.json'
            responses.append(dict(path=live['pending']['path'],payload=live['pending']['payload'],status=599,
                reply=dict(error='receipt_resolved_without_delivery',archive=str(old.relative_to(directory)),sha256=s.sha(old),
                           request_sha256=contract.validate(live['pending']['payload'])),
                applied=dict(claim=False,progress=False,complete=False,solution=False)))
        f.configure_journal(directory/'http-live.json',dict(metadata_sha256=s.sha(directory/c.META),client_id=f.client_id,server=f.url),
                            token,calls,responses,published)
        def interrupted(*_):
            nonlocal cancel
            cancel=True
        handlers={sig:signal.signal(sig,interrupted) for sig in (signal.SIGTERM,signal.SIGINT)}
        environment=p.environment(native,gpu);environment.update(OPENPUZZLE_EXECUTION_SLOT='cuda-0',OPENPUZZLE_VANITY_ATTEMPT=token)
        log=(history/'process.log') if history else directory/'process.log';start=time.monotonic();next_sample=0;last=-1
        with log.open('x') as output:
            process=subprocess.Popen(f.command(resume),env=environment,stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
            try:identity=owner.capture(process.pid)
            except (FileNotFoundError,ProcessLookupError,AttributeError):identity=None
            with guard:transaction['native']=identity;r.atomic(directory/c.ACTIVE,transaction,replace=True)
            while process.poll() is None:
                if cancel and (native/'controls').is_dir() and not (native/'controls').is_symlink():(native/'controls/cancel').touch(mode=0o600,exist_ok=True)
                elapsed=time.monotonic()-start;e.require(elapsed<180,'Owned receipt session exceeded time limit')
                if not terminal_only and elapsed>=next_sample:
                    sample=s.telemetry(gpu);next_sample=elapsed+1
                    e.require(sample['temperature_c']<85 and abs(sample['power_limit_w']-limit)<=.5,'Receipt thermal/power context changed')
                    e.require(not any(Path('/proc',str(pid)).exists() and not p.m.descendant(pid,process.pid) for pid in s.compute_pids(gpu)),
                              'Another process uses the selected GPU')
                path=native/'coordinator-state.txt';checked=initial
                if path.is_file():checked=max(initial,int(path.read_text().splitlines()[2].split()[2]))
                if checked!=last:
                    c.emit(status='running',checked=checked,total=c.TOTAL,resumed_from=initial,
                           speed_mkeys=(checked-initial)/max(elapsed,.001)/1e6,production_enabled=False);last=checked
                time.sleep(.05)
        f.close();e.require(process.returncode>=0,'Native process lost; explicit owner snapshot recovery required')
        e.require(f._lock.acquire(blocking=False),'Receipt HTTP request still active; review required')
        try:
            e.require(not f.errors,'Receipt peer rejected a request: '+repr(f.errors))
            e.require(f.requests,'No HTTP evidence for current native invocation')
            current=proof.live(directory,meta,token,transaction['journal_sha256'])
            e.require(current['pending'] is None and current['requests']==f.base_requests+f.requests
                      and current['responses']==f.base_responses+f.responses,'Unsettled owned receipt HTTP journal')
            result,disposition,_=checked_evidence(directory,meta,current,token=token,code=process.returncode)
            if resume:e.require(result['checked']>=initial,'Receipt resume progress regressed')
            publish(directory,current);seal(directory,result,disposition,token)
            (directory/c.ACTIVE).unlink();active=False
        finally:f._lock.release()
        c.emit(status='completed' if result['phase']=='done' else 'cancelled' if disposition['kind']=='paused' else 'recovery-pending',
               checked=result['checked'],total=c.TOTAL,report=str(directory),next_claim_blocked=True,production_enabled=False)
        return process.returncode
    except BaseException:
        if active and process is not None and (process.poll() is None or process.returncode>=0):
            with guard:transaction['review_required']=True;r.atomic(directory/c.ACTIVE,transaction,replace=True)
        raise
    finally:
        if process is not None:s.stop_owned(process)
        f.close()
        if active and process is None and resume and not f.requests:
            previous=history/'before/http-live.json'
            if previous.exists():c.local.private_write(directory/'http-live.json',previous.read_bytes())
            if state['cold']:c.local.private_write(directory/c.ACTIVE,(history/'before'/c.ACTIVE).read_bytes())
            else:(directory/c.ACTIVE).unlink()
        for sig,handler in handlers.items():signal.signal(sig,handler)
