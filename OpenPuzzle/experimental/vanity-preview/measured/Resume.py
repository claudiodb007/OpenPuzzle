"""GPL-3.0. Explicit resume of sealed local preview runs. Never production.

The measured CLI/adapter/engine remain unchanged. Their persisted binding pins
the loopback port and paths, so resume rebinds that exact port or refuses before
touching the interrupted run. No claim is accepted by the resumed fixture.
"""
import fcntl
from collections import Counter
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import tempfile
import time
import uuid
import Evidence as e
import Measure as m
import Support as s

TOTAL = 2**36
BATCH = 1024*256*1024
SEAL = 'resume-state.json'


def atomic(path, data, *,replace=False):
    path=Path(path)
    e.require(not path.is_symlink(),'Unsafe publication path')
    fd,name=tempfile.mkstemp(prefix='.'+path.name+'-',dir=path.parent)
    try:
        with os.fdopen(fd,'w') as f:
            json.dump(data,f,sort_keys=True,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
        if replace:os.replace(name,path)
        else:os.link(name,path,follow_symlinks=False)
        fd=os.open(path.parent,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(fd)
        finally:os.close(fd)
    finally:
        if os.path.exists(name):os.unlink(name)


def private_file(root,relative):
    path=root/relative
    e.require(path.resolve()==path and path.is_file() and path.stat().st_uid==os.getuid()
              and path.stat().st_mode&0o077==0,'Missing/unsafe local record: '+relative)
    return path


def owned(store,directory):
    import Profile as p
    store=p.private_directory(store);directory=Path(directory)
    e.require(directory.is_absolute() and directory.parent==store and directory.resolve()==directory
              and re.fullmatch(r'run-[0-9a-f-]{36}',directory.name),'Run must belong to this preview store')
    p.private_directory(directory)
    return directory


def protocol_coverage(directory,checked,launches):
    """Check each exact interval, contiguous batch acknowledgements and terminal."""
    attempts=list((directory/'state/assignment').glob('attempt-*'))
    e.require(len(attempts)==launches and launches>0,'Native launch/attempt count differs')
    intervals=[];acknowledged=[]
    launch_logs=list((directory/'state').glob('launch-*.log'))
    e.require(len(launch_logs)==launches,'Missing native adapter launch journal')
    for log in launch_logs:
        text=private_file(directory,str(log.relative_to(directory))).read_text()
        results=re.findall(r'^OPV_CLIENT RESULT (complete|interrupted) checked=([0-9]+) resumed=([0-9]+) safe_stop=([01]) next_blocked=([01])$',text,re.M)
        e.require(len(results)==1,'Missing/ambiguous durable adapter result')
        state,end,start,safe,blocked=results[0];end=int(end);start=int(start)
        e.require(0<=start<=end<=TOTAL and (state!='complete' or end==TOTAL)
                  and (state!='interrupted' or blocked=='1'),'Invalid adapter result')
        acknowledged.append((start,end-start,state))
    for attempt in attempts:
        e.require(attempt.resolve()==attempt and attempt.is_dir(),'Unsafe native attempt')
        result=private_file(directory,str((attempt/'solutions.txt').relative_to(directory)))
        e.require(result.stat().st_size==0,'Solution-bearing run must be reviewed, not resumed')
        log=private_file(directory,str((attempt/'engine.log').relative_to(directory)))
        lines=[line.split() for line in log.read_text().splitlines() if line.startswith('OPV1 ')]
        e.require(lines and len(lines[0])==5 and lines[0][:2]==['OPV1','BEGIN'],'Missing native range identity')
        start=int(lines[0][2],16);count=int(lines[0][3]);batch=int(lines[0][4])
        e.require(start>=s.BASE and start+count==s.BASE+TOTAL and batch==BATCH,'Native interval differs')
        n=0;terminal=False;complete=False
        for row in lines[1:]:
            e.require(not terminal and len(row)==3,'Invalid native protocol tail')
            if row[1]=='PROGRESS':
                e.require(n<count and int(row[2])==n+min(batch,count-n),'Native progress gap/overlap')
                n=int(row[2])
            else:
                e.require(row[1] in ('STOPPED','COMPLETE') and int(row[2])==n
                          and (row[1]!='COMPLETE' or n==count),'Invalid native terminal')
                terminal=True
                complete=row[1]=='COMPLETE'
        # Cooperative cancellation may close the stream before STOPPED arrives.
        # The adapter's durable interrupted result and last acknowledged batch
        # are authoritative; completion still requires the engine terminal.
        intervals.append((start-s.BASE,n,'complete' if complete else 'interrupted'))
    e.require(Counter(intervals)==Counter(acknowledged),'Engine batches and durable adapter results differ')
    offset=0
    for start,n,state in sorted(intervals):
        e.require(start==offset,'Native attempts overlap or skip keys')
        offset+=n
    e.require(offset==checked,'Native coverage differs from checkpoint')


def snapshot(directory,record,paths):
    names=['SUMMARY.json','argv.json','http-requests.json','state/http-state.txt',
           'state/assignment/checkpoint.txt','state/targets.txt','process.log']
    for marker in ('cancel','safe-stop'):
        if (directory/'controls'/marker).exists() or (directory/'controls'/marker).is_symlink():names.append('controls/'+marker)
    for pattern in ('state/assignment/attempt-*/engine.log','state/assignment/attempt-*/solutions.txt','state/launch-*.log'):
        names.extend(str(p.relative_to(directory)) for p in directory.glob(pattern))
    pins={name:s.sha(private_file(directory,name)) for name in sorted(names)}
    summary=e.read_json(directory/'SUMMARY.json')
    e.require(summary==dict(status=summary.get('status'),production_enabled=False,profile_binding=record['binding_sha256'])
              and summary['status'] in ('completed','cancelled'),'Run summary/profile differs')
    argv=e.read_json(directory/'argv.json')
    e.require(isinstance(argv,list) and all(isinstance(x,str) for x in argv) and argv.count('--server')==1,'Invalid stored command')
    url=argv[argv.index('--server')+1]
    e.require(re.fullmatch(r'http://127\.0\.0\.1:[1-9][0-9]{0,4}',url) and int(url.rsplit(':',1)[1])<=65535,'Nonlocal/invalid endpoint')
    expected,binding=m.command(paths,directory,TOTAL,record['uuid'],url)
    e.require(argv==expected,'Run arguments differ from measured executables/target')
    cp=(directory/'state/assignment/checkpoint.txt').read_text().split()
    e.require(len(cp)==9 and cp[:6]==['OPV-CHECKPOINT-1',binding,f'{s.BASE:064x}',str(TOTAL),'1024','stop']
              and cp[8] in ('0','1'),'Checkpoint binding differs')
    checked=int(cp[6]);done=summary['status']=='completed'
    e.require((done and checked==TOTAL and cp[7]=='complete') or
              (not done and 0<=checked<TOTAL and checked%BATCH==0 and cp[7]=='interrupted'),'Invalid terminal checkpoint')
    state=(directory/'state/http-state.txt').read_text().split()
    e.require(len(state)==6 and state[:5]==['OPV-HTTP-1',binding,'done' if done else 'assigned','measurement-local','1']
              and state[5].isdigit(),'HTTP state differs')
    launches=int(state[5]);protocol_coverage(directory,checked,launches)
    expected_target='OPV-TARGETS-1\np2pkh '+s.address(s.key_hash(s.BASE-1))+'\n'
    e.require((directory/'state/targets.txt').read_text()==expected_target,'Synthetic public target changed')
    calls=e.read_json(directory/'http-requests.json');client='opv-local-'+binding[:24]
    e.require(isinstance(calls,list) and calls and calls[0]==dict(path='/api/range/claim',payload=dict(client_id=client,puzzle=20,execution_slot='cuda-0',backend='cuda',target_duration_minutes=1,speed_mkeys=0)),'Original single claim missing')
    last=-BATCH;complete=0
    for call in calls[1:]:
        e.require(set(call)=={'path','payload'},'Invalid HTTP history')
        data=call['payload'];n=int(data.get('keys_checked','-1'))
        if call['path']=='/api/range/progress':
            e.require(not complete and data==dict(assignment_id='measurement-local',client_id=client,status='running',speed_mkeys=0,keys_checked=str(n))
                      and n in (last,last+BATCH) and 0<=n<=checked,'HTTP progress gap/regression')
            last=n # Each resume probes the same durable offset once before continuing.
        else:
            e.require(call==dict(path='/api/range/complete',payload=dict(assignment_id='measurement-local',client_id=client,exit_code=0,status='completed',keys_checked=str(TOTAL)))
                      and done and last==TOTAL and complete==0,'Unexpected/duplicate claim or completion')
            complete+=1
    e.require(complete==int(done) and last==checked,'HTTP/native acknowledged offsets differ')
    return dict(schema='openpuzzle_local_resume_v1',profile_binding=record['binding_sha256'],
                run_root=str(directory),status=summary['status'],checked=checked,total=TOTAL,
                safe_stop=cp[8]=='1',binding=binding,launches=launches,files=pins)


def inspect(store,directory,*,expected=None):
    import Profile as p
    directory=owned(store,directory)
    e.require(not (directory/'resume-active.json').exists(),'Incomplete resume transaction; preserve for review')
    record,_,paths=p.read_selected(store)
    seal=private_file(directory,SEAL)
    if expected is not None:e.require(s.sha(seal)==expected,'Run changed after confirmation; refresh the list')
    before=e.read_json(seal);actual=snapshot(directory,record,paths)
    e.require(before==actual,'Run data changed after checkpoint publication')
    e.require(actual['status']=='cancelled' and not actual['safe_stop'],'Only interrupted runs without Safe Stop can resume')
    e.require((directory/'controls/cancel').is_file() and not (directory/'controls/safe-stop').exists(),'Interrupted control marker missing/changed')
    return actual,record,paths


def new_test(store,directory):
    import Profile as p
    owned_parent=Path(directory)
    e.require(owned_parent.parent==Path(store) and re.fullmatch(r'run-[0-9a-f-]{36}',owned_parent.name),'New run must belong to preview storage')
    code=p.local_run(store,directory)
    record,_,paths=p.read_selected(store)
    atomic(Path(directory)/SEAL,snapshot(Path(directory),record,paths))
    return code


def list_runs(store):
    import Profile as p
    store=p.private_directory(store);rows=[]
    for directory in sorted(store.glob('run-*'),reverse=True):
        if len(rows)>=100:break
        # Never import historical 0.8 runs automatically. Only this preview seals runs.
        if not (directory/SEAL).exists():continue
        try:
            value,_,_=inspect(store,directory)
            rows.append(dict(path=str(directory),available=True,checked=value['checked'],total=TOTAL,state_sha256=s.sha(directory/SEAL)))
        except Exception as exc:
            rows.append(dict(path=str(directory),available=False,reason=str(exc)))
    print(json.dumps(dict(status='local-runs',runs=rows)),flush=True)


def native_idle(directory):
    for name in ('state/owner.lock','state/assignment/owner.lock'):
        fd=os.open(private_file(directory,name),os.O_RDONLY|os.O_NOFOLLOW)
        try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        finally:os.close(fd)


def resume(store,directory,expected):
    import Profile as p
    directory=owned(store,directory)
    before,record,paths=inspect(store,directory,expected=expected)
    native_idle(directory)
    gpu,limit=p.hardware(record)
    argv=e.read_json(directory/'argv.json');url=argv[argv.index('--server')+1];initial=before['checked']
    # Binding the fixed port must succeed before a single run file is changed.
    server=m.LocalMeasurementServer(TOTAL,s.address(s.key_hash(s.BASE-1)),port=int(url.rsplit(':',1)[1]),
                                    client='opv-local-'+before['binding'][:24],initial_checked=initial)
    process=None;archive=None;active=False;cancel=False;old={};finished=False
    def interrupted(*_):
        nonlocal cancel
        cancel=True
    try:
        inspect(store,directory,expected=expected);native_idle(directory)
        history=directory/'resume-history'
        if not history.exists():history.mkdir(mode=0o700)
        p.private_directory(history)
        archive=history/(str(before['launches'])+'-'+str(uuid.uuid4()));archive.mkdir(mode=0o700)
        for name in [SEAL,*before['files']]:
            destination=archive/'before'/name;destination.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
            shutil.copyfile(private_file(directory,name),destination);destination.chmod(0o600)
            e.require(s.sha(destination)==s.sha(directory/name),'Archive copy differs')
        atomic(directory/'resume-active.json',dict(archive=str(archive),prior_state_sha256=expected))
        active=True
        old={sig:signal.signal(sig,interrupted) for sig in (signal.SIGTERM,signal.SIGINT)}
        # Only the verified run's cancel marker is cleared, after preserving it.
        (directory/'controls/cancel').unlink()
        start=time.monotonic();next_sample=0;last=-1
        with (archive/'process.log').open('x') as log:
            process=subprocess.Popen(argv,env=p.environment(directory,gpu),cwd=directory,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            while process.poll() is None:
                if cancel:(directory/'controls/cancel').touch(mode=0o600,exist_ok=True)
                elapsed=time.monotonic()-start;e.require(elapsed<180,'Resume exceeded local time limit')
                if elapsed>=next_sample:
                    sample=s.telemetry(gpu);next_sample=elapsed+1
                    e.require(sample['temperature_c']<85 and abs(sample['power_limit_w']-limit)<=.5,'Thermal/power context changed')
                    e.require(not any(not m.descendant(pid,process.pid) for pid in s.compute_pids(gpu)),'Another process uses the GPU')
                checked=server.progress[-1] if server.progress else initial
                if checked!=last:
                    print(json.dumps(dict(status='running',checked=checked,total=TOTAL,resumed_from=initial,
                         speed_mkeys=(checked-initial)/max(elapsed,.001)/1e6)),flush=True);last=checked
                time.sleep(.05)
        e.require(not server.errors,'Resume HTTP refused: '+repr(server.errors))
        if process.returncode==0:
            server.verify();status='completed'
        elif process.returncode==130 and (cancel or (directory/'controls/cancel').exists()):status='cancelled'
        else:raise RuntimeError('Resume failed; preserve active transaction and inspect '+str(archive))
        e.require(not any(row['path']=='/api/range/claim' for row in server.requests),'Resume attempted another claim')
        calls=e.read_json(archive/'before/http-requests.json')+server.requests
        atomic(directory/'http-requests.json',calls,replace=True)
        atomic(directory/'SUMMARY.json',dict(status=status,production_enabled=False,profile_binding=record['binding_sha256']),replace=True)
        after=snapshot(directory,record,paths)
        e.require(after['launches']==before['launches']+1 and after['checked']>=initial,'Resume did not continue the existing assignment')
        atomic(archive/'result.json',dict(status=status,resumed_from=initial,checked=after['checked'],new_claims=0,
                                        new_completions=server.completions,process_log_sha256=s.sha(archive/'process.log')))
        atomic(directory/SEAL,after,replace=True)
        (directory/'resume-active.json').unlink();active=False;finished=True
        print(json.dumps(dict(status=status,report=str(directory),resumed_from=initial,checked=after['checked'],next_claim_blocked=True)),flush=True)
        return 0 if status=='completed' else 130
    finally:
        if process is not None:s.stop_owned(process)
        server.close()
        if archive is not None:
            atomic(archive/'http-requests.json',server.requests)
            atomic(archive/'http-errors.json',server.errors)
        # A failed spawn has not touched the native assignment: restore its
        # original marker and allow a retry; keep the attempted-resume archive.
        if active and process is None:
            shutil.copyfile(archive/'before/controls/cancel',directory/'controls/cancel')
            (directory/'controls/cancel').chmod(0o600)
            (directory/'resume-active.json').unlink()
        for sig,handler in old.items():signal.signal(sig,handler)
