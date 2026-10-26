#!/usr/bin/env python3
"""GPL-3.0. Measured candidate import and isolated execution; no production route."""
import argparse,ctypes,fcntl,hashlib,importlib.util,json,math,os,signal,stat,subprocess,sys,time
from pathlib import Path
import Evidence as e
import Measure as m
import Support as s

def file(path):
    p=Path(path)
    e.require(p.is_absolute() and not p.is_symlink() and p.is_file(),'Absolute regular file required: '+str(p))
    return p

def candidate(path):
    path=file(path);record=e.read_json(path)
    e.require(record.get('schema')=='openpuzzle_vanity_cli_measured_profile_v1' and record.get('status')=='measured_candidate','Unsupported measured profile')
    proof=file(record['evidence']['path']);e.require(s.sha(proof)==record['evidence']['sha256'],'Measurement evidence changed')
    evidence=e.read_json(proof)
    e.require(evidence.get('version')=='0.7.0' and evidence.get('gpu')=='NVIDIA GeForce RTX 4070 SUPER','Unsupported measurement/GPU')
    e.require(evidence.get('threads')==256 and evidence.get('points')==1024,'Unexpected measured geometry')
    uuid=evidence['uuid'];e.require(len(uuid)==40 and uuid.startswith('GPU-') and all(c in '0123456789abcdefABCDEF-' for c in uuid[4:]),'Invalid physical GPU UUID')
    e.require(type(evidence['power_limit_w']) in (int,float) and math.isfinite(evidence['power_limit_w']) and 0<evidence['power_limit_w']<1000,'Invalid power context')
    e.require(set(evidence['executables'])=={'cli','adapter','engine','bitcrack'},'Incomplete measured executable binding')
    paths={k:file(v['path']) for k,v in evidence['executables'].items()}
    for p in paths.values():e.require(os.access(p,os.X_OK),'Bound binary is not executable')
    e.require(record==e.profile(evidence,proof,paths),'Candidate statistics, metadata or binding differ from measurement')
    # Validate all six immutable per-run records; no rates reconstructed from rounded output.
    for row in evidence['measurements']:
        label=row['label'];e.require(label==f"measure-{row['pair']}-{row['engine']}",'Invalid measurement label')
        e.require(e.read_json(proof.parent/label/'result.json')==row,'Per-run evidence differs')
    src=evidence['source_checksums'];e.require(s.sha(file(src['path']))==src['sha256'] and e.read_json(src['path'])==e.read_json(Path(__file__).with_name('HTTP_SOURCE.json')),'Measured CLI source fingerprint differs')
    prior=Path(evidence['prior_report']);e.require(s.sha(file(prior/'SUMMARY.json'))==evidence['prior_summary_sha256'],'Prior HTTP validation changed')
    return record,evidence,paths

def private_directory(path,create=False):
    p=Path(path);e.require(p.is_absolute() and not p.is_symlink(),'Absolute non-symlink directory required')
    if create and not p.exists():p.mkdir(mode=0o700) # Parent must already exist.
    st=p.stat();e.require(p.is_dir() and st.st_uid==os.getuid() and stat.S_IMODE(st.st_mode)&0o077==0,'Owner-only directory required')
    return p

def atomic_selection(store,value):
    dest=store/'selected.json';pending=store/'selected.pending'
    if dest.exists() or dest.is_symlink():
        e.require(not dest.is_symlink(),'Existing unsafe selection preserved')
        read_selected(store) # Refuse to overwrite damaged or stale prior selection.
    fd=os.open(pending,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
    with os.fdopen(fd,'w') as f:json.dump(value,f,sort_keys=True);f.write('\n');f.flush();os.fsync(f.fileno())
    os.replace(pending,dest)
    fd=os.open(store,os.O_DIRECTORY);os.fsync(fd);os.close(fd)

def import_profile(source,store):
    store=private_directory(store,True);record,proof,paths=candidate(source)
    target=store/(record['binding_sha256']+'.json')
    if target.exists() or target.is_symlink():
        e.require(not target.is_symlink() and e.read_json(target)==record and target.stat().st_mode&0o077==0,'Existing profile differs or is unsafe; preserved')
    else:e.publish_private(target,record)
    atomic_selection(store,dict(schema='openpuzzle_vanity_selection_v1',profile=target.name,sha256=s.sha(target)))
    return record

def read_selected(store):
    store=private_directory(store);selection=file(store/'selected.json')
    e.require(selection.stat().st_mode&0o077==0,'Selection is not private')
    value=e.read_json(selection);name=value['profile']
    e.require(set(value)=={'schema','profile','sha256'} and value['schema']=='openpuzzle_vanity_selection_v1' and len(name)==69 and name.endswith('.json') and all(c in '0123456789abcdef' for c in name[:-5]),'Invalid selected profile')
    path=file(store/name);e.require(path.stat().st_mode&0o077==0 and s.sha(path)==value['sha256'],'Selected profile changed')
    record,proof,paths=candidate(path);e.require(name==record['binding_sha256']+'.json','Selected binding differs')
    return record,proof,paths

def emit(record):
    print(json.dumps(dict(status='ready',engine='Vanity',backend='CUDA',gpu=record['gpu'],uuid=record['uuid'],median_mkeys=record['median_mkeys'],planning_mkeys=record['planning_mkeys'],grid=record['grid'],threads=record['threads'],points=record['points'],samples=record['samples'],production_enabled=False)),flush=True)

def environment(directory,uuid):
    env=os.environ.copy()
    for key in list(env):
        if key.startswith(('OPENPUZZLE_','OPV_','CUDA_')) or key in ('LD_PRELOAD','PYTHONPATH'):env.pop(key,None)
    env.update(HOME=str(directory/'home'),XDG_CONFIG_HOME=str(directory/'home/.config'),XDG_DATA_HOME=str(directory/'home/.local/share'),CUDA_VISIBLE_DEVICES=uuid,CUDA_DEVICE_ORDER='PCI_BUS_ID',CUDA_CACHE_PATH=str(directory/'cuda-cache'))
    return env

def hardware(record,strict=True):
    uuid=record['uuid'];rows=[r for r in s.smi(['uuid','name','driver_version']) if r[0]==uuid]
    e.require(len(rows)==1 and rows[0][1]==record['gpu'] and (not strict or rows[0][2]==record['driver']),'Measured GPU/driver differs; measure a new profile')
    s.require_idle(uuid);limit=s.telemetry(uuid)['power_limit_w']
    e.require(not strict or abs(limit-record['power_limit_w'])<=.5,'Power limit differs from measured profile; measure again')
    return uuid,limit

def local_run(store,directory):
    record,proof,paths=read_selected(store);uuid,limit=hardware(record)
    directory=Path(directory);e.require(directory.is_absolute() and not directory.exists() and not directory.is_symlink(),'Fresh local run directory required')
    directory.mkdir(mode=0o700);(directory/'controls').mkdir();(directory/'home').mkdir()
    count=2**36 # ~26s on the measured GPU, one absent public target outside Puzzle 71.
    server=m.LocalMeasurementServer(count,s.address(s.key_hash(s.BASE-1)));p=None;status='failed';cancel=False
    def interrupted(*_):
        nonlocal cancel
        cancel=True
    old={sig:signal.signal(sig,interrupted) for sig in (signal.SIGTERM,signal.SIGINT)}
    try:
        argv,binding=m.command(paths,directory,count,uuid,server.url);s.write_json(directory/'argv.json',argv)
        start=time.monotonic();next_sample=0;last=-1
        with (directory/'process.log').open('w') as log:
            p=subprocess.Popen(argv,env=environment(directory,uuid),cwd=directory,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            while p.poll() is None:
                if cancel:
                    (directory/'controls/cancel').touch(mode=0o600,exist_ok=True)
                elapsed=time.monotonic()-start;e.require(elapsed<180,'Local run exceeded 180 seconds')
                if elapsed>=next_sample:
                    sample=s.telemetry(uuid);next_sample=elapsed+1
                    e.require(sample['temperature_c']<85 and abs(sample['power_limit_w']-limit)<=.5,'Thermal or power context changed')
                    e.require(not any(not m.descendant(pid,p.pid) for pid in s.compute_pids(uuid)),'Another process uses the GPU')
                checked=server.progress[-1] if server.progress else 0
                if checked!=last:
                    print(json.dumps(dict(status='running',checked=checked,total=count,speed_mkeys=checked/max(elapsed,.001)/1e6)),flush=True);last=checked
                time.sleep(.05)
        if p.returncode==0:
            server.verify()
            cp=(directory/'state/assignment/checkpoint.txt').read_text().split()
            e.require(cp==['OPV-CHECKPOINT-1',binding,f'{s.BASE:064x}',str(count),str(m.GRID),'stop',str(count),'complete','0'] or cp==['OPV-CHECKPOINT-1',binding,f'{s.BASE:064x}',str(count),str(m.GRID),'stop',str(count),'complete','1'],'Exact terminal checkpoint missing')
            e.require((directory/'state/http-state.txt').read_text().split()==['OPV-HTTP-1',binding,'done','measurement-local','1','1'],'Terminal assignment state differs')
            status='completed'
        elif p.returncode==130 and (cancel or (directory/'controls/cancel').exists()):status='cancelled'
        else:raise RuntimeError('Local CLI failed; inspect '+str(directory/'process.log'))
        print(json.dumps(dict(status=status,report=str(directory),next_claim_blocked=True)),flush=True)
        return 130 if status=='cancelled' else 0
    finally:
        if p is not None:s.stop_owned(p)
        server.close();s.write_json(directory/'http-requests.json',server.requests)
        s.write_json(directory/'SUMMARY.json',dict(status=status,production_enabled=False,profile_binding=record['binding_sha256']))
        for sig,handler in old.items():signal.signal(sig,handler)

def benchmark(store):
    record,proof,paths=read_selected(store);hardware(record,False)
    spec=importlib.util.spec_from_file_location('MeasuredRepeat',Path(__file__).with_name('Medir-CLI.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    before=sys.argv;old={sig:signal.signal(sig,lambda *_:(_ for _ in ()).throw(KeyboardInterrupt())) for sig in (signal.SIGTERM,signal.SIGINT)}
    try:
        sys.argv=['Medir-CLI.py','--run','--prior-report',proof['prior_report'],'--bitcrack',str(paths['bitcrack'])]
        report=module.main();e.require(report is not None and (report/'vanity-cli-measured.profile.json').is_file(),'No stable profile produced; prior selection preserved')
        emit(import_profile(report/'vanity-cli-measured.profile.json',store))
    finally:
        sys.argv=before
        for sig,handler in old.items():signal.signal(sig,handler)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=['measured-import','measured-show','measured-test','measured-benchmark']);parser.add_argument('--store',required=True,type=Path);parser.add_argument('--from',dest='source',type=Path);parser.add_argument('--run-root',type=Path);parser.add_argument('--enable-experimental-vanity-local',action='store_true');args=parser.parse_args()
    os.umask(0o077)
    parent=os.getppid();e.require(ctypes.CDLL(None).prctl(1,signal.SIGTERM,0,0,0)==0 and os.getppid()==parent and parent!=1,'Preview owner is unavailable')
    e.require(args.action=='measured-show' or args.enable_experimental_vanity_local,'Explicit local opt-in required')
    e.require((args.source is not None)==(args.action=='measured-import'),'Unexpected/missing import path')
    e.require((args.run_root is not None)==(args.action=='measured-test'),'Unexpected/missing run path')
    store=private_directory(args.store,True)
    fd=os.open(store/'session.lock',os.O_RDWR|os.O_CREAT|os.O_NOFOLLOW,0o600)
    try:
        fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        if args.action=='measured-import':emit(import_profile(args.source,store))
        elif args.action=='measured-show':emit(read_selected(store)[0])
        elif args.action=='measured-test':return local_run(store,args.run_root)
        else:benchmark(store)
    finally:os.close(fd)
    return 0
if __name__=='__main__':
    try:sys.exit(main())
    except KeyboardInterrupt:print('Cancelled; previous profile preserved.',flush=True);sys.exit(130)
    except Exception as exc:print('Measured Vanity refused: '+str(exc),file=sys.stderr);sys.exit(2)
