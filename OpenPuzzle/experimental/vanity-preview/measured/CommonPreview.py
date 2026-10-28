"""Open the existing main UI on its ordinary measured Vanity route.
The existing local peer/journal supplies one bounded acceptance assignment.
No production connection, benchmark, engine installation or automatic GPU launch.
"""
import argparse,configparser,fcntl,hashlib,io,json,os,re,shlex,subprocess,sys,tempfile,time,uuid
from pathlib import Path
import CommonProfile
import Profile
import Support as support
from CommonAcceptance import CommonAcceptanceServer,private_json,save
from LocalCoordinator import START,require,private_write


def sha(path):
    path=Path(path);require(path.is_absolute() and path.resolve()==path and path.is_file(),'Missing/unsafe acceptance binary')
    return support.sha(path)


def private_dir(path):
    path=Path(path);require(path.is_absolute() and path.resolve()==path and path.is_dir() and path.stat().st_uid==os.getuid()
                            and not path.stat().st_mode&0o077,'Private direct acceptance directory required')
    return path


def environment(root,summary,server):
    env=os.environ.copy()
    for key in list(env):
        if key.startswith(('OPENPUZZLE_','OPV_','CUDA_','GIT_')) or key in ('LD_PRELOAD','PYTHONPATH','PYTHONOPTIMIZE'):env.pop(key,None)
    env.update(HOME=str(root/'home'),XDG_CONFIG_HOME=str(root/'home/.config'),XDG_DATA_HOME=str(root/'home/.local/share'),
               OPENPUZZLE_CLI=summary['cli'],OPENPUZZLE_SERVER_URL=server,
               OPENPUZZLE_PUZZLE_DIR=str(root/'puzzles'),OPENPUZZLE_VANITY_COMMON_PROFILE=summary['run_profile']['path'],
               PYTHONDONTWRITEBYTECODE='1')
    return env


def stop_owned(root,summary,env):
    data=root/'home/.local/share/OpenPuzzle'
    if not (data/'runtime.pid').exists():return
    result=subprocess.run([summary['cli'],'stop'],env=env,capture_output=True,text=True,timeout=15)
    require(result.returncode==0,'Unable to pause the owned preview runtime; local files preserved')
    deadline=time.monotonic()+15
    while (data/'runtime.pid').exists() and time.monotonic()<deadline:time.sleep(.05)
    require(not (data/'runtime.pid').exists(),'Preview runtime still owns its session; files preserved')


def review(root,context,peer):
    state=root/'home/.local/share/OpenPuzzle/client.state';workspace=root/'home/.local/share/OpenPuzzle/assignments'/context['assignment']
    cp=workspace/'vanity-native/checkpoint.txt';checked=0;status='not_started'
    if cp.exists():
        words=cp.read_text().split();require(len(words)==9 and words[0]=='OPV-CHECKPOINT-1' and int(words[3])==context['count'],'Acceptance checkpoint needs review')
        checked=int(words[6]);require(0<=checked<=context['count'],'Acceptance checkpoint escaped');status=words[7]
    result=peer.review();phase='ready'
    if result['completions']:
        require(result['claims']==1 and result['claim_requests']==1 and status=='complete' and checked==context['count'] and not state.exists() and not result['errors'],'Incomplete ordinary acceptance evidence')
        phase='done'
    elif state.exists():phase='paused' if status in ('running','interrupted') and checked<context['count'] else 'review'
    result.update(phase=phase,native_checked=checked,total=context['count'],workspace=str(workspace),native_status=status)
    save(root/'REVIEW.json',result);return result


def main(report):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',action='store_true',help='Open the main UI; only its Start button launches the measured GPU engine')
    parser.add_argument('--new',action='store_true',help='Select a fresh bounded local session; previous sessions stay preserved')
    args=parser.parse_args();os.umask(0o077);report=private_dir(report);summary=private_json(report/'SUMMARY.json')
    require(summary['version']=='0.20.1' and summary['status']=='prepared_common_route_validated'
            and summary['scope_suites']==14 and summary['production_enabled'] is False,'Prepared ordinary route report required')
    source=private_dir(Path(summary['source']));manifest=private_json(report/'kit/SOURCE.json')
    require(sha(report/'kit/SOURCE.json')==summary['source_manifest_sha256'],'Prepared source manifest changed')
    for name,digest in manifest.items():require(sha(source/name)==digest,'Prepared source changed: '+name)
    for kind in ('cli','ui','adapter'):require(sha(summary[kind])==summary[kind+'_sha256'],'Prepared binary changed: '+kind)
    require(CommonProfile.bind(Path(summary['profile_store']),Path(summary['cli']),Path(summary['adapter']),Path(summary['run_profile']['path']))==summary['run_profile'],'Bound native profile changed')
    record,_,paths=Profile.read_selected(Path(summary['profile_store']))
    print('OpenPuzzle — interface principal + RunSession normal + motor Vanity medido. Produção desativada.',flush=True)
    print('Iniciar usa a GPU medida. Parar conserva a atribuição. Fechar pausa esta sessão local; o mesmo comando permite reabrir e Retomar.',flush=True)
    print('Paragem segura termina esta atribuição. Sem benchmark, instalação ou alterações no servidor.',flush=True)
    if not args.run:
        print('Verificação concluída; interface e GPU não foram iniciadas. Para abrir:',flush=True)
        print(shlex.join(['python3',str(report/'Abrir-OpenPuzzle-Vanity.py'),'--run']),flush=True);return 0
    runs=report/'acceptance';runs.mkdir(mode=0o700,exist_ok=True);private_dir(runs)
    fd=os.open(runs/'preview.lock',os.O_RDWR|os.O_CREAT|os.O_NOFOLLOW,0o600)
    with os.fdopen(fd,'a+b') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);selected=runs/'selected.json'
        fresh=args.new or not selected.exists()
        root=Path(tempfile.mkdtemp(prefix='run-',dir=runs)) if fresh else private_dir(Path(private_json(selected)['path']))
        require(root.parent==runs and re.fullmatch(r'run-[A-Za-z0-9_-]+',root.name),'Acceptance session escapes report')
        profile_hash=summary['run_profile']['sha256'];context_path=root/'context.json'
        if fresh:
            for name in ('home','home/.config/OpenPuzzle','puzzles'):(root/name).mkdir(mode=0o700,parents=True,exist_ok=True)
            client=str(uuid.uuid4());(root/'home/.config/OpenPuzzle/client.id').write_text(client+'\n')
            count=min(2**36,int(record['grid'])*256*1024*8192);target=support.address(support.key_hash(START-2))
            context=dict(schema='openpuzzle_common_acceptance_v1',client=client,assignment=str(uuid.uuid4()),count=count,
                         target=target,profile_sha256=profile_hash,port=0)
            save(root/'puzzles/69.json',dict(number=69,address=target,search_mode='linear',keyspace=f'{START:x}:{2**69-1:x}'))
        else:
            context=private_json(context_path);require(context['schema']=='openpuzzle_common_acceptance_v1' and context['profile_sha256']==profile_hash,'Acceptance profile changed')
        identity={key:context[key] for key in ('schema','client','assignment','count','target','profile_sha256')}
        peer=CommonAcceptanceServer(root,cli=Path(summary['cli']),adapter=Path(summary['adapter']),engine=paths['engine'],
            count=context['count'],grid=int(record['grid']),target=context['target'],planning=float(record['planning_mkeys']),gpu_uuid=record['uuid'],
            client_id=context['client'],assignment_id=context['assignment'],port=int(context['port']),journal=root/'http-live.json',identity=identity)
        env=environment(root,summary,peer.url)
        try:
            query=subprocess.run(['nvidia-smi','--query-gpu=index,uuid','--format=csv,noheader'],capture_output=True,text=True,timeout=10)
            rows=[line.split(',') for line in query.stdout.splitlines()];matches=[int(row[0].strip()) for row in rows if len(row)==2 and row[1].strip()==record['uuid']]
            require(query.returncode==0 and len(matches)==1,'Measured GPU is missing/ambiguous')
            settings=root/'home/.config/OpenPuzzle Project';settings.mkdir(mode=0o700,parents=True,exist_ok=True)
            settings_path=settings/'OpenPuzzle.conf';options=configparser.ConfigParser(interpolation=None,strict=False);options.optionxform=str
            if settings_path.exists():options.read(settings_path,encoding='utf-8')
            if not options.has_section('ui'):options.add_section('ui');options.set('ui','language','pt')
            if not options.has_section('run'):options.add_section('run')
            for key,value in (('mode','5'),('puzzle','69'),('device',str(matches[0]))):options.set('run',key,value)
            buffer=io.StringIO();options.write(buffer);private_write(settings_path,buffer.getvalue().encode())
            if fresh:
                context['port']=peer.server.server_port;save(context_path,context);save(selected,dict(path=str(root)))
            print('Sessão local:',root,flush=True)
            with (root/'ui.log').open('a') as log:
                result=subprocess.run([summary['ui']],env=env,stdout=log,stderr=subprocess.STDOUT)
            stop_owned(root,summary,env)
            value=review(root,context,peer);print('REVISÃO:',json.dumps(value,sort_keys=True),flush=True)
            print('Relatório:',root/'REVIEW.json',flush=True)
            return result.returncode
        finally:
            try:stop_owned(root,summary,env)
            finally:peer.close()
