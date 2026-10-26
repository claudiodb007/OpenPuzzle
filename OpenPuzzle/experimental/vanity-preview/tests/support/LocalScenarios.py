import hashlib,json,os,signal,subprocess,time
from pathlib import Path
import Support as s
BASE=s.BASE
def save(p,x):s.write_json(p,x)
def wif(key):
    payload=b'\x80'+key.to_bytes(32,'big')+b'\x01'
    raw=payload+hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]
    n=int.from_bytes(raw,'big');out=''
    while n:n,r=divmod(n,58);out=s.ALPHABET[r]+out
    return out

def read_result(root):
    return dict(line.split('=',1) for line in (root/'result.txt').read_text().splitlines())
def private_files(root):
    for p in root.rglob('*'):
        if p.is_file() and p.stat().st_mode&0o077:raise RuntimeError('Ficheiro de estado não privado: '+str(p))
def descendant(pid,parent):
    seen=set()
    while pid>1 and pid not in seen:
        if pid==parent:return True
        seen.add(pid)
        try:pid=int(Path('/proc',str(pid),'stat').read_text().split(') ',1)[1].split()[1])
        except (FileNotFoundError,ProcessLookupError):return True
    return False

class Scenarios:
    def __init__(self,report,adapter,client,engine,env,uuid=None,limit=None):
        self.report=report;self.adapter=adapter;self.client=client;self.engine=engine;self.env=env;self.uuid=uuid;self.limit=limit;self.rows=[]
    def setup(self,name,count,grid):
        d=self.report/name;d.mkdir();(d/'controls').mkdir();(d/'home').mkdir()
        key=BASE if name=='solution' else BASE-1
        (d/'known-keys.txt').write_text(f'{key:064x}\n')
        (d/'expected.txt').write_text(f'{key:064x} {s.key_hash(key).hex()} {s.address(s.key_hash(key))} {wif(key)}\n')
        bind=lambda start:hashlib.sha256(json.dumps(dict(engine=s.sha(self.engine),uuid=self.uuid or 'fake',start=start,count=count,grid=grid,scenario=name,targets=s.sha(d/'known-keys.txt')),sort_keys=True).encode()).hexdigest()
        args=[self.client,'vanity-preview',self.adapter,self.engine,d/'state',d/'controls',d/'known-keys.txt',d/'expected.txt',f'{BASE:x}',str(count),str(grid),bind(BASE),bind(BASE+count),name,'--enable-experimental-vanity-local']
        save(d/'argv.json',[str(x) for x in args]);return d,args
    def call(self,label,d,args,expected=0,kill=False):
        env=self.env.copy();env.update(HOME=str(d/'home'),XDG_CONFIG_HOME=str(d/'home/.config'),XDG_DATA_HOME=str(d/'home/.local/share'))
        if self.uuid:s.require_idle(self.uuid)
        log=d/(label+'.log');samples=[];killed=False;started=time.monotonic();next_sample=0
        print('Atribuição sintética:',label,flush=True)
        with log.open('w') as out:
            p=subprocess.Popen([str(a) for a in args],env=env,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
            try:
                while p.poll() is None:
                    elapsed=time.monotonic()-started
                    if elapsed>180:raise RuntimeError('Local runtime fixture timeout')
                    if self.uuid and elapsed>=next_sample:
                        sample=s.telemetry(self.uuid);samples.append(sample);next_sample=elapsed+1
                        if sample['temperature_c']>=85 or abs(sample['power_limit_w']-self.limit)>.5:raise RuntimeError('Temperatura/limite de potência interrompeu o teste')
                        if any(not descendant(pid,p.pid) for pid in s.compute_pids(self.uuid)):raise RuntimeError('Outro processo está a usar a GPU')
                    heartbeat=d/'state/local-heartbeat.txt'
                    if kill and not killed and heartbeat.exists():
                        payload=dict(line.split('=',1) for line in heartbeat.read_text().splitlines())
                        if int(payload['keys_checked'])>0:p.kill();killed=True
                    time.sleep(.02)
            finally:s.stop_owned(p)
        save(d/(label+'-telemetry.json'),samples)
        if p.returncode!=expected:raise RuntimeError(f'{label}: código {p.returncode}, esperado {expected}; consulta {log}')
        if kill and not killed:raise RuntimeError('Owner death was not exercised')
        if kill:
            # Confirm that the adapter inherited parent-death cancellation.
            deadline=time.monotonic()+10
            cp=d/'state/local-a/checkpoint.txt'
            while time.monotonic()<deadline:
                if cp.exists() and cp.read_text().split()[-2]=='interrupted':break
                time.sleep(.05)
            else:raise RuntimeError('Killed local owner did not stop its adapter')
        if self.uuid:s.require_idle(self.uuid)
        if expected!=20 and not kill:private_files(d/'state')
        row=dict(label=label,status='passed',exit_code=p.returncode,result=read_result(d/'state') if (d/'state/result.txt').exists() else None)
        self.rows.append(row);print('  PASSOU',flush=True);return row
    def repeat_blocked(self,label,d,args):
        before={str(p):s.sha(p) for p in (d/'state').rglob('*') if p.is_file()}
        self.call(label,d,args,20)
        after={str(p):s.sha(p) for p in (d/'state').rglob('*') if p.is_file()}
        if before!=after:raise RuntimeError('Blocked repeat changed preserved assignment data')
    def run_all(self,real):
        grid=32 if real else 1;short=3*grid*256*1024+7;long=2**34+7 if real else 12*262144+7
        for name in ('normal','safe-stop','idle-safe-stop','completion-retry','heartbeat-retry'):
            d,args=self.setup(name,short,grid);row=self.call(name,d,args);r=row['result']
            expected=0 if name=='idle-safe-stop' else 1 if name=='safe-stop' else 2
            if (int(r['claims']),int(r['launches']),int(r['completed']))!=(expected,expected,expected):raise RuntimeError('Unexpected claim/relaunch/completion count: '+name)
            events=(d/'state/events.log').read_text().splitlines() if (d/'state/events.log').exists() else []
            if name=='completion-retry' and (int(r['ack_attempts'])!=3 or events.count('completion-retry')!=1):raise RuntimeError('Completion retry was not exercised')
            if name=='heartbeat-retry' and (int(r['heartbeats'])!=2 or events.index('claim-local-b')<events.index('idle-heartbeat-accepted')):raise RuntimeError('Second claim preceded availability acknowledgement')
            if name in ('normal','safe-stop'):self.repeat_blocked(name+'-repeat',d,args)
        for name in ('cancel-resume','owner-crash'):
            d,args=self.setup(name,long,grid);self.call(name,d,args,-signal.SIGKILL if name=='owner-crash' else 130,kill=name=='owner-crash')
            row=self.call(name+'-recovered',d,args);r=row['result']
            if (int(r['claims']),int(r['launches']),int(r['completed']))!=(2,3,2):raise RuntimeError('Recovery requested duplicate work or relaunched a completed assignment')
            self.repeat_blocked(name+'-repeat',d,args)
        d,args=self.setup('lease-rejected',long,grid);r=self.call('lease-rejected',d,args,2)['result']
        if (r['claims'],r['completed'],r['phase'])!=('1','0','rejected'):raise RuntimeError('Rejected assignment was completed or replaced')
        self.repeat_blocked('lease-rejected-repeat',d,args)
        d,args=self.setup('solution',short,grid);r=self.call('solution',d,args)['result']
        if (r['claims'],r['completed'],r['phase'])!=('1','0','solution'):raise RuntimeError('Solution requested more work')
        exports=list((d/'home/OpenPuzzle-Solutions').rglob('wallet-import.txt'))
        if len(exports)!=1 or wif(BASE) not in exports[0].read_text():raise RuntimeError('Shared protected solution exporter failed')
        private_files(d/'home/OpenPuzzle-Solutions')
        if wif(BASE) in (d/'solution.log').read_text() or f'{BASE:064x}' in (d/'state/local-heartbeat.txt').read_text():raise RuntimeError('Private synthetic key leaked into public output')
        self.repeat_blocked('solution-repeat',d,args)
        return self.rows
