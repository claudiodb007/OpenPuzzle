"""Local acceptance peer for the ordinary client, reusing the existing HTTP journal.
No coordinator, kernels, tests imports or production connections live here.
"""
import json,math,os,re,stat,threading,uuid
from pathlib import Path
from LocalCoordinator import LocalAssignmentServer,private_write,require,START


def private_json(path):
    path=Path(path);info=path.lstat()
    require(path.is_absolute() and path.resolve()==path and stat.S_ISREG(info.st_mode)
            and info.st_uid==os.getuid() and not info.st_mode&0o077 and info.st_size<=8*1024*1024,'Unsafe local acceptance record')
    return json.loads(path.read_text())


def save(path,value):
    private_write(path,(json.dumps(value,indent=2,sort_keys=True)+'\n').encode())


def uuid4(value):
    require(isinstance(value,str) and str(uuid.UUID(value))==value and uuid.UUID(value).version==4,'Local acceptance requires UUIDv4 identities')
    return value


class CommonAcceptanceServer(LocalAssignmentServer):
    """One ordinary assignment. The native client owns execution and recovery."""
    def __init__(self,root,*,cli,adapter,engine,count,grid,target,planning,gpu_uuid,
                 client_id,assignment_id,port=0,journal=None,identity=None):
        self.ready=threading.Event()
        client_id=uuid4(client_id);assignment_id=uuid4(assignment_id)
        require(type(count) is int and count>0 and count<=min(2**36,grid*256*1024*8192),'Acceptance scope exceeds measured budget')
        super().__init__(root,cli=cli,adapter=adapter,engine=engine,count=count,grid=grid,
                         segment=min(count,2**35),target=target,planning=planning,gpu_uuid=gpu_uuid,port=port)
        self.client_id=client_id;self.assignment_id=assignment_id;self.puzzle=69
        try:
            requests=[];responses=[]
            if journal is not None and Path(journal).exists():
                prior=private_json(journal)
                require(prior['schema']=='openpuzzle_http_live_v1' and prior['identity']==identity
                        and prior['pending'] is None and len(prior['requests'])==len(prior['responses']),
                        'Incomplete/different acceptance journal; preserved for review')
                requests,responses=prior['requests'],prior['responses']
                for request,response in zip(requests,responses):
                    require(request['path']==response['path'] and request['payload']==response['payload'],'Acceptance journal mismatch')
                    payload=response['payload'];require(payload.get('client_id')==self.client_id,'Acceptance client changed')
                    if response['path'].startswith('/api/range/') and response['path']!='/api/range/claim':
                        self.assignment_identity(payload)
                    applied=response['applied'];self.claims+=int(applied['claim']);self.completions+=int(applied['complete']);self.reports+=int(applied['solution'])
                    if applied['progress']:
                        n=int(payload['keys_checked']);require(0<=n<=count and (not self.progress or n>=self.progress[-1]),'Acceptance progress changed');self.progress.append(n)
                require(self.claims<=1 and self.completions<=1 and self.reports<=1,'Acceptance effects duplicated')
            if journal is not None:self.configure_journal(journal,identity,'common-client',requests,responses,lambda _:None)
            self.ready.set()
        except BaseException:
            self.ready.set();self.close();raise

    def assignment_identity(self,body):
        require(body.get('client_id')==self.client_id and body.get('assignment_id')==self.assignment_id,'Acceptance assignment/client differs')

    def handle(self,path,body):
        require(self.ready.wait(3),'Acceptance peer is not ready')
        self.seen[path]=self.seen.get(path,0)+1
        require(body.get('client_id')==self.client_id,'Acceptance client differs')
        if path in ('/api/client/register','/api/client/heartbeat'):return 200,dict(success=True)
        if path=='/api/range/claim':
            require(body.get('puzzle')==self.puzzle and body.get('backend')=='cuda'
                    and body.get('execution_slot')=='primary' and 1<=int(body.get('target_duration_minutes',0))<=360
                    and abs(float(body.get('speed_mkeys',-1))-self.planning)<.000001
                    and self.count<=int(body.get('requested_keys',0))<=min(2**40,self.grid*256*1024*8192),'Ordinary claim differs from bound acceptance')
            if self.completions or self.reports:return 404,dict(error='no_assignment_available',message='Local acceptance is complete')
            if not self.claims:self.claims=1
            return 200,dict(assignment_id=self.assignment_id,puzzle=self.puzzle,range_id=37,target=self.target,
                search_mode='linear',start=f'{START:064x}',end=f'{START+self.count-1:064x}')
        self.assignment_identity(body);require(self.claims==1,'No acceptance assignment claimed')
        if path=='/api/range/progress':
            if self.completions or self.reports:return 409,dict(error='assignment_not_active')
            n=int(body.get('keys_checked',-1));speed=float(body.get('speed_mkeys',-1))
            require(body.get('status')=='running' and math.isfinite(speed) and speed>=0 and 0<=n<=self.count
                    and (not self.progress or n>=self.progress[-1]),'Acceptance progress escaped/regressed')
            self.progress.append(n);return 200,dict(success=True,status='running')
        if path=='/api/range/complete':
            require(body.get('status')=='completed' and body.get('exit_code')==0 and int(body.get('keys_checked',-1))==self.count and not self.reports,'Acceptance completion is not exact')
            prior=bool(self.completions);self.complete_attempts+=1;self.completions=1
            return 200,dict(success=True,status='completed',already_finalized=prior)
        if path=='/api/puzzle/report-solution':
            require(set(body)=={'assignment_id','client_id'} and not self.completions,'Private result must use existing metadata-only notice')
            self.reports=1;return 200,dict(success=True)
        raise RuntimeError('Unsupported acceptance endpoint')

    def command(self,*_,**__):
        raise RuntimeError('Use the ordinary RunSession/main controls; no acceptance execution coordinator')

    def review(self):
        return dict(claims=self.claims,completions=self.completions,reports=self.reports,
                    checked=self.count if self.completions else (self.progress[-1] if self.progress else 0),errors=self.errors,
                    claim_requests=sum(r['path']=='/api/range/claim' for r in self.base_requests+self.requests),
                    production_enabled=False)
