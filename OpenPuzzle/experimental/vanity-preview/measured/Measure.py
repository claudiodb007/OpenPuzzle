"""Bounded loopback measurement fixture; never a production coordinator."""
import hashlib,json,math,os,signal,statistics,subprocess,threading,time
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import Support as s
from Evidence import require,GRID,PARAMS
BASE=s.BASE
def descendant(pid,parent):
    seen=set()
    while pid>1 and pid not in seen:
        if pid==parent:return True
        seen.add(pid)
        try:pid=int(Path('/proc',str(pid),'stat').read_text().split(') ',1)[1].split()[1])
        except (FileNotFoundError,ProcessLookupError):return True
    return False
class LocalMeasurementServer:
    def __init__(self,count,address):
        self.count=count;self.address=address;self.requests=[];self.errors=[];self.claims=0;self.completions=0;self.progress=[];self.client=None;outer=self
        class Handler(BaseHTTPRequestHandler):
            protocol_version='HTTP/1.1'
            def log_message(self,*_):pass
            def do_POST(self):
                try:
                    length=int(self.headers['Content-Length']);require(0<length<=8192,'Oversized fixture request')
                    data=json.loads(self.rfile.read(length));outer.requests.append(dict(path=self.path,payload=data))
                    if self.path=='/api/range/claim':
                        require(outer.claims==0 and not outer.completions,'Duplicate claim');outer.claims+=1
                        require(set(data)=={'client_id','puzzle','execution_slot','backend','target_duration_minutes','speed_mkeys'} and data['puzzle']==20 and data['backend']=='cuda' and data['execution_slot']=='cuda-0' and data['speed_mkeys']==0,'Unexpected claim payload')
                        outer.client=data['client_id']
                        result=dict(assignment_id='measurement-local',puzzle=20,range_id=1,target=outer.address,search_mode='linear',required_backend='cuda',start=f'{BASE:064x}',end=f'{BASE+count-1:064x}')
                    else:
                        require(data.get('assignment_id')=='measurement-local' and data.get('client_id')==outer.client,'Wrong local assignment/client')
                        if self.path=='/api/range/progress':
                            require(set(data)=={'assignment_id','client_id','status','speed_mkeys','keys_checked'} and data['status']=='running' and data['speed_mkeys']==0 and outer.claims==1 and outer.completions==0,'Unexpected progress payload')
                            n=int(data['keys_checked']);require(str(n)==data['keys_checked'] and 0<=n<=count and (not outer.progress or n>outer.progress[-1]),'Invalid/duplicate/regressing progress')
                            outer.progress.append(n)
                        elif self.path=='/api/range/complete':
                            require(set(data)=={'assignment_id','client_id','exit_code','status','keys_checked'} and data['exit_code']==0 and data['status']=='completed' and data['keys_checked']==str(count),'Wrong completion')
                            require(outer.claims==1 and outer.completions==0 and outer.progress and outer.progress[-1]==count,'Premature/duplicate completion');outer.completions+=1
                        else:raise RuntimeError('Unexpected endpoint in absent-target measurement')
                        result={'success':True}
                    payload=json.dumps(result,separators=(',',':')).encode();self.send_response(200);self.send_header('Content-Length',str(len(payload)));self.send_header('Connection','close');self.end_headers();self.wfile.write(payload);self.close_connection=True
                except Exception as e:
                    outer.errors.append(str(e));self.close_connection=True
        self.server=ThreadingHTTPServer(('127.0.0.1',0),Handler);self.thread=threading.Thread(target=lambda:self.server.serve_forever(poll_interval=.05),daemon=True);self.thread.start();self.url=f'http://127.0.0.1:{self.server.server_port}'
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join(timeout=5)
    def verify(self):
        require(not self.errors,'HTTP fixture refused: '+repr(self.errors));require(self.claims==1 and self.completions==1,'Expected exactly one claim and completion')
        expected=[0]+list(range(GRID*256*1024,self.count,GRID*256*1024))+[self.count]
        require(self.progress==expected,'HTTP did not receive every exact checkpoint')
def command(paths,directory,count,uuid,url):
    options=dict(engine=paths['engine'],engine_sha256=s.sha(paths['engine']),adapter=paths['adapter'],adapter_sha256=s.sha(paths['adapter']),root=directory/'state',controls=directory/'controls',server=url,start=f'{BASE:x}',count=count,grid=GRID,target_kind='p2pkh',target=s.address(s.key_hash(BASE-1)),gpu_uuid=uuid)
    argv=[str(paths['cli']),'vanity-preview','http-test',*[str(x) for k,v in options.items() for x in ('--'+k.replace('_','-'),v)],'--enable-experimental-vanity-local']
    binding=''.join('--'+k.replace('_','-')+'='+str(v)+'\n' for k,v in sorted(options.items(),key=lambda item:'--'+item[0].replace('_','-')))
    return argv,hashlib.sha256(binding.encode()).hexdigest()
def timed(argv,directory,env,uuid,limit,timeout=900):
    done=threading.Event();clock={};samples=[]
    with (directory/'process.log').open('w') as log:
        start=time.perf_counter();p=subprocess.Popen([str(x) for x in argv],cwd=directory,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
        def wait():p.wait();clock['end']=time.perf_counter();done.set()
        waiter=threading.Thread(target=wait,daemon=True);waiter.start();next_sample=0;next_status=15
        try:
            while not done.wait(.05):
                elapsed=time.perf_counter()-start
                require(elapsed<timeout,'Measurement exceeded maximum duration')
                if uuid and elapsed>=next_sample:
                    t=s.telemetry(uuid);t['elapsed_seconds']=elapsed;samples.append(t);next_sample=elapsed+1
                    require(t['temperature_c']<85,'GPU atingiu 85 °C; teste interrompido')
                    require(abs(t['power_limit_w']-limit)<=.5,'Limite de potência mudou')
                    require(not any(not descendant(pid,p.pid) for pid in s.compute_pids(uuid)),'Outro processo está a usar a GPU')
                if elapsed>=next_status:print(f'  A medir: {int(elapsed)} s',flush=True);next_status=elapsed+15
        finally:s.stop_owned(p);waiter.join(timeout=10);s.write_json(directory/'telemetry.json',samples)
        require(done.is_set() and p.returncode==0,f'Command failed ({p.returncode}); see {directory / "process.log"}')
    return clock['end']-start,samples
def verify_cli(directory,count,binding):
    state=(directory/'state/http-state.txt').read_text().split();require(state==['OPV-HTTP-1',binding,'done','measurement-local','1','1'],'HTTP terminal state does not prove one completed launch')
    workspace=directory/'state/assignment';cp=(workspace/'checkpoint.txt').read_text().split()
    require(cp==['OPV-CHECKPOINT-1',binding,f'{BASE:064x}',str(count),str(GRID),'stop',str(count),'complete','0'],'Exact native checkpoint missing')
    attempts=list(workspace.glob('attempt-*'));require(len(attempts)==1,'Expected one fresh native attempt')
    results=attempts[0]/'solutions.txt';require(results.is_file() and not results.is_symlink() and results.stat().st_mode&0o077==0 and results.stat().st_size==0,'Absent target produced an unexpected/unsafe result')
    batch=GRID*256*1024
    wanted=[f'OPV1 BEGIN {BASE:064x} {count} {batch}']+[f'OPV1 PROGRESS {n}' for n in range(batch,count,batch)]+[f'OPV1 PROGRESS {count}']
    if count<=65536:wanted.append(f'OPV1 TRACE {count}')
    wanted.append(f'OPV1 COMPLETE {count}')
    actual=[line for line in (attempts[0]/'engine.log').read_text().splitlines() if line.startswith('OPV1 ')]
    require(actual==wanted,'Native interval/progress/terminal proof differs')
    log=(directory/'process.log').read_text();require('OPV_HTTP RESULT done launches=1 next_claim_blocked=1' in log,'CLI terminal confirmation missing')
def cli_fixture(report,label,paths,count,env,uuid,limit,pair=None):
    d=report/label;d.mkdir();(d/'controls').mkdir();(d/'home').mkdir()
    env=dict(env,HOME=str(d/'home'),XDG_CONFIG_HOME=str(d/'home/.config'),XDG_DATA_HOME=str(d/'home/.local/share'))
    server=LocalMeasurementServer(count,s.address(s.key_hash(BASE-1)))
    try:
        argv,binding=command(paths,d,count,uuid,server.url);s.write_json(d/'argv.json',argv)
        if limit is not None:s.require_idle(uuid)
        print('Medição:',label,flush=True);elapsed,samples=timed(argv,d,env,uuid if limit is not None else None,limit)
        server.verify();verify_cli(d,count,binding)
    finally:
        server.close();s.write_json(d/'http-requests.json',server.requests);s.write_json(d/'http-errors.json',server.errors)
    if limit is not None:s.require_idle(uuid)
    return result(d,label,'vanity-cli',count,elapsed,samples,limit,pair)
def bit_fixture(report,label,paths,count,env,uuid,limit,pair=None,correctness=False):
    require(count%math.prod(PARAMS)==0,'BitCrack comparison interval must be aligned')
    d=report/label;d.mkdir();inside=set();outside={BASE-1}
    if correctness:
        bit=math.prod(PARAMS);vanity=GRID*256*1024
        inside={BASE+n for n in (0,1,511,512,bit-1,bit,vanity-1,vanity,count-2,count-1) if n<count}
        outside|={BASE+count,BASE+count+1}
    require(len(inside|outside)<=16,'Avoid the unresolved BitCrack large-target Bloom path')
    targets=d/'targets.txt';targets.write_text(''.join(s.address(s.key_hash(k))+'\n' for k in sorted(inside|outside)))
    b,t,p=PARAMS;argv=[paths['bitcrack'],'-d','0','-b',str(b),'-t',str(t),'-p',str(p),'-c','--keyspace',f'{BASE:x}:{BASE+count-1:x}','-i',targets,'-o',d/'found.txt']
    s.write_json(d/'argv.json',[str(x) for x in argv]);s.require_idle(uuid);print('Medição:',label,flush=True)
    elapsed,samples=timed(argv,d,env,uuid,limit);output=(d/'found.txt').read_text() if (d/'found.txt').exists() else ''
    s.validate_result('bitcrack',(d/'process.log').read_text(),output,count,inside);s.require_idle(uuid)
    return result(d,label,'bitcrack',count,elapsed,samples,limit,pair)
def result(d,label,engine,count,elapsed,samples,limit,pair):
    row=dict(label=label,engine=engine,status='passed',keys=count,wall_seconds=elapsed,wall_mkeys_per_second=count/elapsed/1e6,pair=pair,power_limit_w=limit)
    for field in ('temperature_c','power_w','fan_percent','sm_clock_mhz','memory_clock_mhz'):
        values=[t[field] for t in samples if t.get(field) is not None];row['mean_'+field]=statistics.mean(values) if values else None
    s.write_json(d/'result.json',row);print(f'  PASSOU: {row["wall_mkeys_per_second"]:.2f} MKey/s',flush=True);return row
