#!/usr/bin/env python3
"""Owned local coordinator via RunSession; durable desktop Stop/Resume, never production."""
import argparse
import ctypes
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time
import threading
import uuid
import Profile as p
import Resume as r
import Evidence as e
import Support as s
import LocalCoordinator as local
import CoordinatorRecovery as recovery
import OwnerRecovery as owner
from LocalCoordinator import LocalAssignmentServer

TOTAL = 2**36
SEGMENT = 2**35
SEAL = 'coordinator-seal.json'
ACTIVE = 'coordinator-active.json'
META = 'coordinator-metadata.json'


def emit(**values):
    print(json.dumps(values,ensure_ascii=False),flush=True)


def native_command(server,cli,resume=False):
    e.require(server.cli==cli,'Local coordinator CLI differs')
    return server.command(resume)


def native_idle(directory):
    path=directory/'native/owner.lock'
    if not path.exists():return
    r.private_file(directory,'native/owner.lock')
    fd=os.open(path,os.O_RDWR|os.O_NOFOLLOW)
    try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
    finally:os.close(fd)


def pins(directory):
    result={}
    for path in directory.rglob('*'):
        relative=path.relative_to(directory)
        if relative.parts[0]=='coordinator-history':continue
        e.require(not path.is_symlink(),'Unsafe coordinator evidence path')
        if path.is_file() and relative.parts[0] not in (SEAL,ACTIVE):
            result[str(relative)]=s.sha(r.private_file(directory,str(relative)))
    return result


def coverage(directory,meta,http_data=None,cold=False,*,assignment_id="coordinator-assignment-A",journal=None):
    receipt=journal is not None
    if receipt:e.require(meta.get('schema')=='openpuzzle_receipt_desktop_v1' and re.fullmatch(r'[0-9a-f-]{36}',assignment_id),'Receipt coverage binding differs')
    native=directory/'native'
    raw=r.private_file(directory,'native/session-binding.txt').read_text()
    values=meta['native_options']
    expected=''.join(key+'='+values[key]+'\n' for key in sorted(values))+'execution_slot=cuda-0\ncoordinator_cli_sha256='+meta['cli_sha256']+'\n'
    e.require(raw==expected,'Native RunSession binding differs')
    binding=hashlib.sha256(raw.encode()).hexdigest()
    state=r.private_file(directory,'native/coordinator-state.txt').read_text().splitlines()
    e.require(len(state)==4 and state[:2]==['OPV-COORD-1',binding],'Native coordinator state differs')
    phase,offset,checked,launches=state[2].split();offset=int(offset);checked=int(checked);launches=int(launches)
    if receipt and cold and phase=='claiming':
        e.require(offset==checked==launches==0 and shlex.split(state[3])==['','0','0','','','','linear','','']
                  and not list(native.glob('segment-*')) and journal['completions']==0,'Unreviewed claiming state')
        return dict(phase='claiming',checked=0,total=TOTAL,launches=0,claims=1,completions=0)
    e.require(phase in (('assigned','running','paused','done','pending-complete','rejected') if cold else ('paused','done','pending-complete','rejected')) and 0<=offset<=checked<=TOTAL,'Unreviewed/nonresumable native state')
    assignment=shlex.split(state[3])
    e.require(assignment==[assignment_id,'20','37',meta['target'],f'{s.BASE:064x}',f'{s.BASE+TOTAL-1:064x}','linear','cuda',''],'Native assignment metadata changed')
    prefix=0;attempt_count=0
    if receipt:e.require({path.name for path in native.glob('segment-*')} <= {'segment-0','segment-1'},'Unexpected receipt native segment')
    for index in range(2):
        segment=native/('segment-'+str(index))
        if not segment.exists():continue
        e.require(prefix==index*SEGMENT,'Native segment gap')
        cp=r.private_file(directory,str((segment/'checkpoint.txt').relative_to(directory))).read_text().split()
        sub_binding=hashlib.sha256((binding+'\n'+str(index*SEGMENT)).encode()).hexdigest()
        e.require(len(cp)==9 and cp[:6]==['OPV-CHECKPOINT-1',sub_binding,f'{s.BASE+index*SEGMENT:064x}',str(SEGMENT),str(meta['grid']),'stop'] and cp[8] in ('0','1'),'Native checkpoint binding differs')
        done=int(cp[6]);e.require(0<=done<=SEGMENT and cp[7] in ('complete','interrupted'),'Native checkpoint requires review')
        spans=[]
        for attempt in segment.glob('attempt-*'):
            e.require(attempt.is_dir() and attempt.resolve()==attempt,'Unsafe native attempt')
            base=str(attempt.relative_to(directory));result=r.private_file(directory,base+'/solutions.txt')
            e.require(result.stat().st_size==0,'Solution-bearing coordinator run requires review')
            rows=[line.split() for line in r.private_file(directory,base+'/engine.log').read_text().splitlines() if line.startswith('OPV1 ')]
            e.require(rows and len(rows[0])==5 and rows[0][:2]==['OPV1','BEGIN'],'Missing native interval')
            first=int(rows[0][2],16);count=int(rows[0][3]);batch=int(rows[0][4]);start=first-(s.BASE+index*SEGMENT)
            e.require(0<=start<SEGMENT and start+count==SEGMENT and batch==meta['grid']*256*1024,'Native segment interval escaped')
            n=0;terminal=False;complete=False
            for row in rows[1:]:
                e.require(not terminal and len(row)==3,'Invalid native protocol tail')
                if row[1]=='PROGRESS':
                    e.require(n<count and int(row[2])==n+min(batch,count-n),'Native batch gap/overlap');n=int(row[2])
                else:
                    e.require(row[1] in ('COMPLETE','STOPPED') and int(row[2])==n and (row[1]!='COMPLETE' or n==count),'Invalid native terminal')
                    terminal=True;complete=row[1]=='COMPLETE'
            spans.append((start,n,complete));attempt_count+=1
        contiguous=0;completed=False
        for start,n,complete in sorted(spans):
            e.require(start==contiguous,'Native attempts overlap/skip keys');contiguous+=n;completed=complete
        e.require(contiguous==done and (cp[7]!='complete' or completed and done==SEGMENT),'Native checkpoint lacks exact coverage')
        if receipt and done==SEGMENT:e.require(cp[7]=='complete' and completed,'Receipt run lacks exact segment terminal proof')
        prefix+=done
        if done<SEGMENT:
            if receipt:e.require(index==1 or not (native/'segment-1').exists(),'Receipt native segment gap')
            break
    if cold and phase in ('assigned','running'):
        e.require(checked<=prefix,'Native counter exceeds durable coverage')
        checked=prefix;phase='paused'
    e.require(prefix==checked and attempt_count==launches and (launches>0 or phase in ('paused','rejected') and checked==offset==0),'Native cumulative counter/launches differ')
    e.require((phase in ('done','pending-complete') and offset==checked==TOTAL) or (phase in ('paused','rejected') and offset in (0,SEGMENT) and offset<=checked<=offset+SEGMENT) or (phase=='rejected' and offset==checked==TOTAL),'Native phase and progress differ')
    if journal is None:journal=recovery.http_state(directory,meta,checked,phase,http_data)
    return dict(phase=phase,checked=checked,total=TOTAL,launches=launches,claims=1,completions=journal['completions'])



def inspect(store,directory,cli,expected=None,allow_complete=False):
    directory=r.owned(store,directory)
    e.require(not (directory/ACTIVE).exists() and not (directory/ACTIVE).is_symlink(),'Coordinator transaction requires review')
    seal_path=r.private_file(directory,SEAL)
    if expected is not None:e.require(s.sha(seal_path)==expected,'Stale coordinator resume confirmation')
    seal=e.read_json(seal_path)
    e.require(seal.get('schema')=='openpuzzle_coordinator_seal_v2','Coordinator seal schema differs')
    meta,record,paths=bound_metadata(store,directory,cli)
    e.require(seal['files']==pins(directory),'Coordinator evidence changed')
    current=coverage(directory,meta);e.require(seal['native']==current and seal['recovery']==recovery.outcome(directory,meta,current),'Coordinator seal differs')
    e.require(allow_complete or current['phase']!='rejected','Lease/assignment rejected; recovery is blocked')
    e.require(allow_complete or current['phase'] in ('paused','pending-complete'),'Completed coordinator run cannot resume')
    e.require(re.fullmatch(r'http://127\.0\.0\.1:[1-9][0-9]{0,4}',meta['native_options']['--server']) is not None,'Nonlocal coordinator endpoint')
    return seal,meta,record,paths


def bound_metadata(store,directory,cli):
    meta=e.read_json(r.private_file(directory,META));record,proof,paths=p.read_selected(store)
    e.require(meta['schema']=='openpuzzle_coordinator_desktop_v4' and meta['profile_binding']==record['binding_sha256'] and meta['gpu_uuid']==record['uuid'],'Coordinator measured profile differs')
    e.require(meta['cli']==str(cli) and meta['cli_sha256']==s.sha(cli),'Coordinator CLI differs')
    e.require(meta['script_sha256']==s.sha(Path(__file__)) and meta['contract_sha256']==s.sha(Path(local.__file__)) and meta['recovery_sha256']==s.sha(Path(recovery.__file__)) and meta['owner_sha256']==s.sha(Path(owner.__file__)),'Coordinator source differs')
    e.require(meta['grid']==record['grid'] and meta['planning_mkeys']==record['planning_mkeys'],'Coordinator launch profile differs')
    e.require(meta['native_options']['--engine']==str(paths['engine']) and meta['native_options']['--engine-sha256']==s.sha(paths['engine']) and meta['native_options']['--adapter']==str(paths['adapter']) and meta['native_options']['--adapter-sha256']==s.sha(paths['adapter']),'Measured executables differ')
    e.require(re.fullmatch(r'http://127\.0\.0\.1:[1-9][0-9]{0,4}',meta['native_options']['--server']) is not None,'Nonlocal coordinator endpoint')
    return meta,record,paths


def owner_proposal(store,directory,cli):
    directory=r.owned(store,directory)
    meta,record,paths=bound_metadata(store,directory,cli)
    active,live=owner.peer(directory,meta)
    native_idle(directory)
    for lock_path in (directory/'native').glob('segment-*/owner.lock'):
        fd=os.open(r.private_file(directory,str(lock_path.relative_to(directory))),os.O_RDWR|os.O_NOFOLLOW)
        try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        finally:os.close(fd)
    raw=r.private_file(directory,'native/coordinator-state.txt').read_text().splitlines()[2].split()
    e.require(raw[0] not in ('rejected','fresh','claiming','unavailable','solution','reported'),'Native phase is not eligible for owner recovery')
    outcome_path=directory/'native/coordinator-outcome.txt'
    if outcome_path.exists():
        fields=r.private_file(directory,'native/coordinator-outcome.txt').read_text().splitlines()[2].split()
        if len(fields)==10 and fields[-1]==active['owner_token']:
            e.require(fields[0] not in ('lease','review'),'Native refusal requires review; owner recovery blocked')
    native=coverage(directory,meta,(live['requests'],live['responses']),cold=True)
    if native['phase']=='done':
        disposition=recovery.outcome(directory,meta,native,owner_token=active['owner_token'],http_data=(live['requests'],live['responses']))
    else:
        e.require(native['phase'] in ('paused','pending-complete'),'Unreviewed owner recovery phase')
        disposition=dict(kind='owner-loss',acknowledged=recovery.http_state(directory,meta,native['checked'],native['phase'],(live['requests'],live['responses']))['acknowledged'])
    files=pins(directory)
    value=dict(schema='openpuzzle_owner_proposal_v1',native=native,recovery=disposition,files=files,
               active_sha256=s.sha(directory/ACTIVE),seal_sha256=s.sha(directory/SEAL) if (directory/SEAL).exists() else None)
    value['state_sha256']=owner.token(value)
    return value,meta,record,paths,live


def list_runs(store,cli):
    rows=[]
    for directory in sorted(store.glob('run-*')):
        if not (directory/META).exists():continue
        from ReceiptDesktop import is_receipt, row as receipt_row
        if is_receipt(directory):
            rows.append(receipt_row(store,directory,cli));continue
        row=dict(path=str(directory),available=False)
        try:
            seal,meta,record,paths=inspect(store,directory,cli)
            row.update(available=True,checked=seal['native']['checked'],total=TOTAL,state_sha256=s.sha(directory/SEAL),recovery=seal['recovery']['kind'],phase=seal['native']['phase'])
        except Exception as error:
            row['reason']=str(error)
            if (directory/ACTIVE).exists():
                try:
                    proposal,_,_,_,_=owner_proposal(store,directory,cli)
                    row.update(available=True,checked=proposal['native']['checked'],total=TOTAL,state_sha256=proposal['state_sha256'],recovery='owner-loss',phase=proposal['native']['phase'])
                    row.pop('reason',None)
                except Exception as problem:row['reason']='Coordinator transaction requires review: '+str(problem)
        rows.append(row)
    emit(status='local-runs',runs=rows,production_enabled=False)


def execute(store,directory,cli,expected=None):
    store=p.private_directory(store);resume=expected is not None
    record,proof,paths=p.read_selected(store)
    before=meta=live=None;cold=False
    if resume:
        if (directory/ACTIVE).exists():
            try:
                before,meta,record,paths,live=owner_proposal(store,directory,cli)
                e.require(before['state_sha256']==expected,'Stale owner recovery confirmation')
                cold=True
            except Exception as error:raise RuntimeError('Coordinator transaction requires review: '+str(error)) from error
        else:before,meta,record,paths=inspect(store,directory,cli,expected)
        native_idle(directory)
    else:
        e.require(directory.is_absolute() and directory.parent==store and directory.resolve()==directory and re.fullmatch(r'run-[0-9a-f-]{36}',directory.name) and not directory.exists(),'Fresh owned coordinator directory required')
    if cold and before['native']['phase']=='done':
        current,_,_,_,_=owner_proposal(store,directory,cli);e.require(current==before,'Owner evidence changed before adoption')
        history=directory/'coordinator-history'/str(uuid.uuid4());history.mkdir(parents=True,mode=0o700)
        for name in [ACTIVE,*before['files'],*([SEAL] if (directory/SEAL).exists() else [])]:
            target=history/'before'/name;target.parent.mkdir(parents=True,exist_ok=True,mode=0o700);shutil.copyfile(r.private_file(directory,name),target);target.chmod(0o600)
        r.atomic(history/'owner-proposal.json',before)
        r.atomic(directory/'http-requests.json',live['requests'],replace=True);r.atomic(directory/'http-responses.json',live['responses'],replace=True)
        state=coverage(directory,meta);disposition=recovery.outcome(directory,meta,state)
        r.atomic(directory/SEAL,dict(schema='openpuzzle_coordinator_seal_v2',native=state,recovery=disposition,files=pins(directory)),replace=True)
        (directory/ACTIVE).unlink();emit(status='completed',checked=TOTAL,total=TOTAL,report=str(directory),next_claim_blocked=True,production_enabled=False);return 0
    gpu,limit=p.hardware(record)
    initial=before['native']['checked'] if resume else 0
    if not resume:
        directory.mkdir(mode=0o700);(directory/'native').mkdir();(directory/'native/home').mkdir()
    native=directory/'native';port=int(meta['native_options']['--server'].rsplit(':',1)[1]) if resume else 0
    f=LocalAssignmentServer(native,cli=cli,adapter=paths['adapter'],engine=paths['engine'],target=s.address(s.key_hash(s.BASE-1)),count=TOTAL,grid=record['grid'],segment=SEGMENT,planning=record['planning_mkeys'],gpu_uuid=gpu,port=port,client_id=meta['client_id'] if resume else None,resume_only=resume,initial_checked=before['recovery']['acknowledged'] if resume else 0)
    process=None;history=None;cancel=False;active=False;finished=False
    handlers={}
    def interrupted(*_):
        nonlocal cancel
        cancel=True
    try:
        if resume:
            if cold:
                current,_,_,_,_=owner_proposal(store,directory,cli);e.require(current==before,'Owner evidence changed before resume')
            else:inspect(store,directory,cli,expected)
            native_idle(directory)
            command=native_command(f,cli)
            e.require(command==meta['argv'],'Resume coordinator arguments differ')
            history=directory/'coordinator-history'/str(uuid.uuid4());history.mkdir(parents=True,mode=0o700)
            archive_names=[*([SEAL] if (directory/SEAL).exists() else []),*before['files'],*([ACTIVE] if cold else [])]
            for name in archive_names:
                destination=history/'before'/name;destination.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
                shutil.copyfile(r.private_file(directory,name),destination);destination.chmod(0o600)
                e.require(s.sha(destination)==s.sha(directory/name),'Coordinator archive differs')
                if name in before['files']:e.require(s.sha(destination)==before['files'][name],'Owner evidence changed during archive')
            if cold:r.atomic(history/'owner-proposal.json',before)
        else:
            command=native_command(f,cli)
            options=f.options()
            meta=dict(schema='openpuzzle_coordinator_desktop_v4',cli=str(cli),cli_sha256=s.sha(cli),profile_binding=record['binding_sha256'],gpu_uuid=gpu,grid=record['grid'],planning_mkeys=record['planning_mkeys'],target=f.target,client_id=f.client_id,native_options=options,argv=command,script_sha256=s.sha(Path(__file__)),contract_sha256=s.sha(Path(local.__file__)),recovery_sha256=s.sha(Path(recovery.__file__)),owner_sha256=s.sha(Path(owner.__file__)))
            r.atomic(directory/META,meta)
        owner_token=str(uuid.uuid4());active_guard=threading.RLock()
        transaction=dict(schema='openpuzzle_owned_transaction_v1',resume=resume,expected=expected,history=str(history) if history else None,owner=owner.capture(os.getpid()),native=None,owner_token=owner_token,review_required=False)
        r.atomic(directory/ACTIVE,transaction,replace=cold);active=True
        def journal_published(digest):
            with active_guard:
                transaction['journal_sha256']=digest;r.atomic(directory/ACTIVE,transaction,replace=True)
        previous_calls=(live['requests'] if cold else e.read_json(directory/'http-requests.json')) if resume else []
        previous_replies=(live['responses'] if cold else e.read_json(directory/'http-responses.json')) if resume else []
        f.configure_journal(directory/'http-live.json',dict(metadata_sha256=s.sha(directory/META),client_id=meta['client_id'],server=f.url),owner_token,previous_calls,previous_replies,journal_published)
        handlers={sig:signal.signal(sig,interrupted) for sig in (signal.SIGTERM,signal.SIGINT)}
        environment=p.environment(native,gpu);environment['OPENPUZZLE_EXECUTION_SLOT']='cuda-0'
        environment['OPENPUZZLE_VANITY_ATTEMPT']=owner_token
        e.require(environment['CUDA_VISIBLE_DEVICES']==gpu==f.gpu_uuid,'Physical GPU binding differs')
        log_path=(history/'process.log') if history else directory/'process.log'
        start=time.monotonic();last=-1;next_sample=0
        with log_path.open('x') as output:
            process=subprocess.Popen(command+(['--resume'] if resume else []),env=environment,stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
            try:native_identity=owner.capture(process.pid)
            except (FileNotFoundError,ProcessLookupError,AttributeError):native_identity=None
            with active_guard:
                transaction['native']=native_identity;r.atomic(directory/ACTIVE,transaction,replace=True)
            while process.poll() is None:
                if cancel:
                    controls=native/'controls'
                    if controls.is_dir() and not controls.is_symlink():(controls/'cancel').touch(mode=0o600,exist_ok=True)
                elapsed=time.monotonic()-start;e.require(elapsed<180,'Local coordinator exceeded time limit')
                if elapsed>=next_sample:
                    sample=s.telemetry(gpu);next_sample=elapsed+1
                    e.require(sample['temperature_c']<85 and abs(sample['power_limit_w']-limit)<=.5,'Thermal/power context changed')
                    e.require(not any(Path('/proc',str(pid)).exists() and not p.m.descendant(pid,process.pid) for pid in s.compute_pids(gpu)),'Another process uses the GPU')
                checked=f.progress[-1] if f.progress else initial
                state_path=native/'coordinator-state.txt'
                if state_path.is_file():
                    fields=state_path.read_text().splitlines()[2].split()
                    checked=max(initial,int(fields[2]))
                if checked!=last:
                    emit(status='running',checked=checked,total=TOTAL,resumed_from=initial,speed_mkeys=(checked-initial)/max(elapsed,.001)/1e6,production_enabled=False);last=checked
                time.sleep(.05)
        f.close()
        calls=f.base_requests+f.requests
        r.atomic(directory/'http-requests.json',calls,replace=resume)
        responses=f.base_responses+f.responses
        r.atomic(directory/'http-responses.json',responses,replace=resume)
        live_final=e.read_json(r.private_file(directory,'http-live.json'))
        e.require(live_final['pending'] is None and live_final['requests']==calls and live_final['responses']==responses and s.sha(directory/'http-live.json')==transaction['journal_sha256'],'Unsettled live HTTP journal')
        e.require(f.claims==(0 if resume else 1),'Unexpected new local claim')
        e.require(not f.errors,'Local coordinator HTTP rejected: '+repr(f.errors))
        state=coverage(directory,meta)
        disposition=recovery.outcome(directory,meta,state,process.returncode,owner_token)
        if resume:e.require(state['checked']>=initial,'Resume progress regressed')
        seal=dict(schema='openpuzzle_coordinator_seal_v2',native=state,recovery=disposition,files=pins(directory))
        r.atomic(directory/SEAL,seal,replace=resume)
        (directory/ACTIVE).unlink();active=False;finished=True
        emit(status='completed' if state['phase']=='done' else 'cancelled' if disposition['kind']=='paused' else 'lease-rejected' if disposition['kind']=='lease' else 'recovery-pending',checked=state['checked'],total=TOTAL,report=str(directory),next_claim_blocked=True,production_enabled=False)
        return process.returncode
    except BaseException:
        if active and process is not None and (process.poll() is None or process.returncode>=0):
            with active_guard:
                transaction['review_required']=True;r.atomic(directory/ACTIVE,transaction,replace=True)
        raise
    finally:
        if process is not None:s.stop_owned(process)
        f.close()
        if history is not None:
            r.atomic(history/'http-requests.json',f.requests);r.atomic(history/'http-responses.json',f.responses);r.atomic(history/'http-errors.json',f.errors)
        if active and process is None and resume and not f.requests:
            saved=history/'before/http-live.json'
            if saved.exists():local.private_write(directory/'http-live.json',saved.read_bytes())
            if cold:local.private_write(directory/ACTIVE,(history/'before'/ACTIVE).read_bytes())
            else:(directory/ACTIVE).unlink()
        for sig,handler in handlers.items():signal.signal(sig,handler)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('start','resume','runs'));parser.add_argument('--cli',required=True,type=Path);parser.add_argument('--store',required=True,type=Path);parser.add_argument('--run-root',type=Path);parser.add_argument('--expected-state');parser.add_argument('--enable-experimental-vanity-local',action='store_true');args=parser.parse_args()
    os.umask(0o077)
    parent=os.getppid();e.require(ctypes.CDLL(None).prctl(1,signal.SIGTERM,0,0,0)==0 and os.getppid()==parent and parent!=1,'Preview owner unavailable')
    e.require(args.action=='runs' or args.enable_experimental_vanity_local,'Explicit local opt-in required')
    e.require((args.run_root is not None)==(args.action!='runs') and (args.expected_state is not None)==(args.action=='resume'),'Unexpected/missing coordinator run confirmation')
    cli=p.file(args.cli);store=p.private_directory(args.store)
    fd=os.open(store/'session.lock',os.O_RDWR|os.O_CREAT|os.O_NOFOLLOW,0o600)
    try:
        fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        if args.action=='runs':list_runs(store,cli);return 0
        from ReceiptDesktop import enabled, is_receipt, execute as receipt_execute
        if args.action=='start' and enabled(store,cli) or args.action=='resume' and is_receipt(args.run_root):
            return receipt_execute(store,args.run_root,cli,args.expected_state)
        return execute(store,args.run_root,cli,args.expected_state)
    finally:os.close(fd)


if __name__=='__main__':
    try:sys.exit(main())
    except KeyboardInterrupt:print('Coordinator cancelled; existing data preserved.',flush=True);sys.exit(130)
    except Exception as error:print('Coordinator RunSession refused: '+str(error),file=sys.stderr);sys.exit(2)
