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
import uuid
import Profile as p
import Resume as r
import Evidence as e
import Support as s
import LocalCoordinator as local
import CoordinatorRecovery as recovery
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


def coverage(directory,meta):
    native=directory/'native'
    raw=r.private_file(directory,'native/session-binding.txt').read_text()
    values=meta['native_options']
    expected=''.join(key+'='+values[key]+'\n' for key in sorted(values))+'execution_slot=cuda-0\ncoordinator_cli_sha256='+meta['cli_sha256']+'\n'
    e.require(raw==expected,'Native RunSession binding differs')
    binding=hashlib.sha256(raw.encode()).hexdigest()
    state=r.private_file(directory,'native/coordinator-state.txt').read_text().splitlines()
    e.require(len(state)==4 and state[:2]==['OPV-COORD-1',binding],'Native coordinator state differs')
    phase,offset,checked,launches=state[2].split();offset=int(offset);checked=int(checked);launches=int(launches)
    e.require(phase in ('paused','done','pending-complete','rejected') and 0<=offset<=checked<=TOTAL,'Unreviewed/nonresumable native state')
    assignment=shlex.split(state[3])
    e.require(assignment==['coordinator-assignment-A','20','37',meta['target'],f'{s.BASE:064x}',f'{s.BASE+TOTAL-1:064x}','linear','cuda',''],'Native assignment metadata changed')
    prefix=0;attempt_count=0
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
        prefix+=done
        if done<SEGMENT:break
    e.require(prefix==checked and attempt_count==launches and (launches>0 or phase in ('paused','rejected') and checked==offset==0),'Native cumulative counter/launches differ')
    e.require((phase in ('done','pending-complete') and offset==checked==TOTAL) or (phase in ('paused','rejected') and offset in (0,SEGMENT) and offset<=checked<=offset+SEGMENT) or (phase=='rejected' and offset==checked==TOTAL),'Native phase and progress differ')
    journal=recovery.http_state(directory,meta,checked,phase)
    return dict(phase=phase,checked=checked,total=TOTAL,launches=launches,claims=1,completions=journal['completions'])



def inspect(store,directory,cli,expected=None,allow_complete=False):
    directory=r.owned(store,directory)
    e.require(not (directory/ACTIVE).exists() and not (directory/ACTIVE).is_symlink(),'Coordinator transaction requires review')
    seal_path=r.private_file(directory,SEAL)
    if expected is not None:e.require(s.sha(seal_path)==expected,'Stale coordinator resume confirmation')
    seal=e.read_json(seal_path);meta=e.read_json(r.private_file(directory,META));record,proof,paths=p.read_selected(store)
    e.require(seal.get('schema')=='openpuzzle_coordinator_seal_v2','Coordinator seal schema differs')
    e.require(meta['schema']=='openpuzzle_coordinator_desktop_v3' and meta['profile_binding']==record['binding_sha256'] and meta['gpu_uuid']==record['uuid'],'Coordinator measured profile differs')
    e.require(meta['cli']==str(cli) and meta['cli_sha256']==s.sha(cli),'Coordinator CLI differs')
    e.require(meta['script_sha256']==s.sha(Path(__file__)) and meta['contract_sha256']==s.sha(Path(local.__file__)) and meta['recovery_sha256']==s.sha(Path(recovery.__file__)),'Coordinator source differs')
    e.require(meta['grid']==record['grid'] and meta['planning_mkeys']==record['planning_mkeys'],'Coordinator launch profile differs')
    e.require(meta['native_options']['--engine']==str(paths['engine']) and meta['native_options']['--engine-sha256']==s.sha(paths['engine']) and meta['native_options']['--adapter']==str(paths['adapter']) and meta['native_options']['--adapter-sha256']==s.sha(paths['adapter']),'Measured executables differ')
    e.require(seal['files']==pins(directory),'Coordinator evidence changed')
    current=coverage(directory,meta);e.require(seal['native']==current and seal['recovery']==recovery.outcome(directory,meta,current),'Coordinator seal differs')
    e.require(allow_complete or current['phase']!='rejected','Lease/assignment rejected; recovery is blocked')
    e.require(allow_complete or current['phase'] in ('paused','pending-complete'),'Completed coordinator run cannot resume')
    e.require(re.fullmatch(r'http://127\.0\.0\.1:[1-9][0-9]{0,4}',meta['native_options']['--server']) is not None,'Nonlocal coordinator endpoint')
    return seal,meta,record,paths


def list_runs(store,cli):
    rows=[]
    for directory in sorted(store.glob('run-*')):
        if not (directory/META).exists():continue
        row=dict(path=str(directory),available=False)
        try:
            seal,meta,record,paths=inspect(store,directory,cli)
            row.update(available=True,checked=seal['native']['checked'],total=TOTAL,state_sha256=s.sha(directory/SEAL),recovery=seal['recovery']['kind'],phase=seal['native']['phase'])
        except Exception as error:row['reason']=str(error)
        rows.append(row)
    emit(status='local-runs',runs=rows,production_enabled=False)


def execute(store,directory,cli,expected=None):
    store=p.private_directory(store);resume=expected is not None
    record,proof,paths=p.read_selected(store)
    before=meta=None
    if resume:
        before,meta,record,paths=inspect(store,directory,cli,expected)
        native_idle(directory)
    else:
        e.require(directory.is_absolute() and directory.parent==store and directory.resolve()==directory and re.fullmatch(r'run-[0-9a-f-]{36}',directory.name) and not directory.exists(),'Fresh owned coordinator directory required')
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
            inspect(store,directory,cli,expected);native_idle(directory)
            command=native_command(f,cli)
            e.require(command==meta['argv'],'Resume coordinator arguments differ')
            history=directory/'coordinator-history'/str(uuid.uuid4());history.mkdir(parents=True,mode=0o700)
            for name in [SEAL,*before['files']]:
                destination=history/'before'/name;destination.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
                shutil.copyfile(r.private_file(directory,name),destination);destination.chmod(0o600)
                e.require(s.sha(destination)==s.sha(directory/name),'Coordinator archive differs')
        else:
            command=native_command(f,cli)
            options=f.options()
            meta=dict(schema='openpuzzle_coordinator_desktop_v3',cli=str(cli),cli_sha256=s.sha(cli),profile_binding=record['binding_sha256'],gpu_uuid=gpu,grid=record['grid'],planning_mkeys=record['planning_mkeys'],target=f.target,client_id=f.client_id,native_options=options,argv=command,script_sha256=s.sha(Path(__file__)),contract_sha256=s.sha(Path(local.__file__)),recovery_sha256=s.sha(Path(recovery.__file__)))
            r.atomic(directory/META,meta)
        r.atomic(directory/ACTIVE,dict(resume=resume,expected=expected,history=str(history) if history else None));active=True
        handlers={sig:signal.signal(sig,interrupted) for sig in (signal.SIGTERM,signal.SIGINT)}
        environment=p.environment(native,gpu);environment['OPENPUZZLE_EXECUTION_SLOT']='cuda-0'
        owner_token=str(uuid.uuid4());environment['OPENPUZZLE_VANITY_ATTEMPT']=owner_token
        e.require(environment['CUDA_VISIBLE_DEVICES']==gpu==f.gpu_uuid,'Physical GPU binding differs')
        log_path=(history/'process.log') if history else directory/'process.log'
        start=time.monotonic();last=-1;next_sample=0
        with log_path.open('x') as output:
            process=subprocess.Popen(command+(['--resume'] if resume else []),env=environment,stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
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
                if checked!=last:
                    emit(status='running',checked=checked,total=TOTAL,resumed_from=initial,speed_mkeys=(checked-initial)/max(elapsed,.001)/1e6,production_enabled=False);last=checked
                time.sleep(.05)
        f.close()
        calls=(e.read_json(directory/'http-requests.json') if resume else [])+f.requests
        r.atomic(directory/'http-requests.json',calls,replace=resume)
        responses=(e.read_json(directory/'http-responses.json') if resume else [])+f.responses
        r.atomic(directory/'http-responses.json',responses,replace=resume)
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
    finally:
        if process is not None:s.stop_owned(process)
        f.close()
        if history is not None:
            r.atomic(history/'http-requests.json',f.requests);r.atomic(history/'http-responses.json',f.responses);r.atomic(history/'http-errors.json',f.errors)
        if active and process is None and resume:(directory/ACTIVE).unlink()
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
        return execute(store,args.run_root,cli,args.expected_state)
    finally:os.close(fd)


if __name__=='__main__':
    try:sys.exit(main())
    except KeyboardInterrupt:print('Coordinator cancelled; existing data preserved.',flush=True);sys.exit(130)
    except Exception as error:print('Coordinator RunSession refused: '+str(error),file=sys.stderr);sys.exit(2)
