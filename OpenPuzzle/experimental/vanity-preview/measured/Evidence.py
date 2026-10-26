"""Local evidence validation; hashes establish consistency, not remote attestation."""
import hashlib,json,math,os,statistics,subprocess,xml.etree.ElementTree as ET
from pathlib import Path
import Support as s
ROOT=Path(__file__).resolve().parent
BASE='2ad96af0667678dea07679ca9be8806b5ba37df8'
PARAMS=(448,128,1024)
GRID=1024
COMMON_BATCH=math.lcm(math.prod(PARAMS),GRID*256*1024)
WARMUP_COUNT=(2**34//COMMON_BATCH)*COMMON_BATCH
MEASURE_COUNT=(2**38//COMMON_BATCH)*COMMON_BATCH
SPREAD_LIMIT=3.0
def read_json(path):
    path=Path(path)
    if path.is_symlink() or not path.is_file() or path.stat().st_size>4*1024*1024:raise RuntimeError('Missing/unsafe evidence file: '+str(path))
    def unique(pairs):
        result={}
        for k,v in pairs:
            if k in result:raise ValueError('Duplicate JSON field: '+k)
            result[k]=v
        return result
    return json.loads(path.read_text(),object_pairs_hook=unique,parse_constant=lambda value:(_ for _ in ()).throw(ValueError('Invalid number '+value)))
def require(ok,message):
    if not ok:raise RuntimeError(message)
def verify_prior(report):
    report=Path(report).resolve();summary=read_json(report/'SUMMARY.json')
    require(summary.get('version')=='0.6.0' and summary.get('status')=='completed' and summary.get('real_gpu_requested') is True and summary.get('production_enabled') is False,'Complete real-GPU HTTP 0.6.0 report required')
    tree=ET.parse(report/'build/linux-junit.xml');cases=list(tree.iter('testcase'));names={c.get('name') for c in cases}
    require(len(cases)>=183 and len(names)==len(cases) and not any(c.find('failure') is not None or c.find('error') is not None or c.find('skipped') is not None or c.get('status') in ('fail','notrun') for c in cases),'Passed full Linux JUnit is required')
    require({'VanityPreviewModelTests','VanityPreviewProtocolTests','VanityPreviewAdapterTests','VanityPreviewCliTests','VanityPreviewHttpTests'}<=names,'Missing experimental regression suites')
    require(summary.get('linux_suites')==len(cases),'JUnit/summary count differs')
    source=report/'source';pins=read_json(ROOT/'HTTP_SOURCE.json')
    require(pins['base_commit']==BASE and subprocess.check_output(['git','-C',source,'rev-parse','HEAD'],text=True).strip()==BASE,'Unexpected prior source base')
    preserved={}
    for rel,digest in pins['files'].items():
        p=source/rel;require(not p.is_symlink() and s.sha(p)==digest,'Prior validated source changed: '+rel);preserved[str(p)]=digest
    for rel,digest in read_json(ROOT/'ENGINE_SOURCE.json').items():
        p=report/'target-engine/source'/rel;require(not p.is_symlink() and s.sha(p)==digest,'Prior target engine source changed: '+rel);preserved[str(p)]=digest
    labels={f'exact-{n}' for n in (1,2,31,33,255,257,1023,1024,1025,65535)}|{'carry','puzzle71-width','batch-tail','measured-grid-tail'}
    rows=summary.get('target_gpu_results',[])
    require(len(rows)==14 and {r.get('label') for r in rows}==labels and all(r.get('status')=='passed' and r.get('outside_guards')==3 for r in rows),'Complete public-target GPU coverage evidence required')
    require(read_json(report/'target-gpu/gpu-results.json')==rows,'Public-target aggregate changed')
    for row in rows:require(read_json(report/'target-gpu'/row['label']/'result.json')==row,'Public-target result changed')
    http=read_json(report/'real-http/http-results.json')
    require(summary.get('real_http_scenarios')==25 and http.get('tests')==25 and http.get('passed') is True and http.get('real_engine') is True and http.get('failures')==[] and http.get('errors')==[],'25 real-engine HTTP scenarios must pass')
    paths=dict(cli=report/'build/openpuzzle',adapter=report/'build/openpuzzle-vanity-adapter',engine=report/'target-engine/build/vanity-offline')
    for key,p in paths.items():
        require(not p.is_symlink() and p.is_file() and os.access(p,os.X_OK),'Missing prior executable: '+key)
        require(Path(summary[key]).resolve()==p,'Summary executable path differs: '+key)
        digest=s.sha(p)
        if key!='cli':require(summary[key+'_sha256']==digest,'Prior executable changed: '+key)
        preserved[str(p)]=digest
    require(read_json(report/'target-engine/build/build.json')['binary_sha256']==s.sha(paths['engine']),'CUDA build hash differs')
    uuid=summary['gpu_uuid'];require(len(uuid)==40 and uuid.startswith('GPU-') and all(c in '0123456789abcdefABCDEF-' for c in uuid[4:]),'Invalid prior GPU UUID')
    # Match the original independently measured profile too; never replace it.
    previous=Path(summary['prior_cli_report']);previous_summary=read_json(previous/'SUMMARY.json');profile=previous/'preview/vanity.profile'
    require(s.sha(profile)==previous_summary['profile_sha256'],'Previous profile changed; preserved');preserved[str(profile)]=s.sha(profile)
    for p in (report/'SUMMARY.json',report/'build/linux-junit.xml',report/'real-http/http-results.json',report/'target-gpu/gpu-results.json'):
        preserved[str(p)]=s.sha(p)
    return paths,uuid,len(cases),preserved
def verify_preserved(pins):
    for name,digest in pins.items():require(s.sha(Path(name))==digest,'Preserved file changed: '+name)
def metrics(rows):
    require(len(rows)==6 and {r['engine'] for r in rows}=={'bitcrack','vanity-cli'},'Exactly six paired measurements required')
    result={}
    for kind in ('bitcrack','vanity-cli'):
        group=[r for r in rows if r['engine']==kind]
        require(len(group)==3 and {r['pair'] for r in group}=={0,1,2},'Three distinct measured pairs required per engine')
        rates=[]
        for row in group:
            require(row.get('status')=='passed' and row['keys']==MEASURE_COUNT and type(row['wall_seconds']) in (int,float) and math.isfinite(row['wall_seconds']) and row['wall_seconds']>0,'Invalid measured interval/time')
            rate=row['keys']/row['wall_seconds']/1e6
            require(math.isfinite(row['wall_mkeys_per_second']) and abs(rate-row['wall_mkeys_per_second'])<1e-7,'Rate differs from exact keys / elapsed time')
            rates.append(rate)
        median=statistics.median(rates);spread=(max(rates)-min(rates))/median*100
        result[kind]=dict(median_mkeys=median,minimum_mkeys=min(rates),maximum_mkeys=max(rates),spread_percent=spread,stable=spread<=SPREAD_LIMIT)
    return result,(result['vanity-cli']['median_mkeys']/result['bitcrack']['median_mkeys']-1)*100
def profile(evidence,evidence_path,paths):
    measured,gain=metrics(evidence['measurements'])
    require(all(r['stable'] for r in measured.values()),'Dispersion exceeds 3%; measurements retained, no profile created')
    require(evidence['status']=='completed' and evidence['production_enabled'] is False and evidence['count']==MEASURE_COUNT and evidence['grid']==GRID,'Incomplete measurement evidence')
    for kind,p in paths.items():require(s.sha(p)==evidence['executables'][kind]['sha256'],'Executable changed after measurement')
    require(read_json(evidence_path)==evidence,'Measurement evidence changed')
    rate=measured['vanity-cli']
    record=dict(schema='openpuzzle_vanity_cli_measured_profile_v1',status='measured_candidate',engine='vanity-preview',backend='cuda',gpu=evidence['gpu'],uuid=evidence['uuid'],driver=evidence['driver'],power_limit_w=evidence['power_limit_w'],grid=GRID,threads=256,points=1024,samples=3,measurement_kind='exact_interval_over_whole_cli_process_time',median_mkeys=rate['median_mkeys'],minimum_mkeys=rate['minimum_mkeys'],maximum_mkeys=rate['maximum_mkeys'],spread_percent=rate['spread_percent'],planning_margin_percent=3,planning_mkeys=rate['median_mkeys']*.97,executables=evidence['executables'],evidence=dict(path=str(Path(evidence_path).resolve()),sha256=s.sha(evidence_path)),production_enabled=False,installed=False,cli_import_supported=False)
    record['binding_sha256']=hashlib.sha256(json.dumps(record,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    return record
def publish_private(path,data):
    """Publish a complete private JSON without replacing any prior file."""
    path=Path(path);temporary=path.with_name(path.name+'.pending')
    require(path.parent.is_dir() and not path.parent.is_symlink() and path.parent.stat().st_mode&0o077==0,'Private profile directory required')
    flags=os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW
    fd=os.open(temporary,flags,0o600)
    with os.fdopen(fd,'w') as f:f.write(json.dumps(data,indent=2,ensure_ascii=False)+'\n');f.flush();os.fsync(f.fileno())
    os.link(temporary,path,follow_symlinks=False) # Atomic no-replace publication.
    fd=os.open(path.parent,os.O_RDONLY|os.O_DIRECTORY)
    try:os.fsync(fd)
    finally:os.close(fd)
    temporary.unlink()
