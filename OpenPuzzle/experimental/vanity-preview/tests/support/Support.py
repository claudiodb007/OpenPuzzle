#!/usr/bin/env python3
"""GPL-3.0-only. Isolated performance comparison helpers; Python stdlib only.
No network calls, installation, OpenPuzzle config writes or GPU setting writes.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent
P = 2**256 - 2**32 - 977
N = 0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141
G = (0x79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798,
     0x483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8)
ALPHABET = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'
CPP = ['Base58.cpp','IntGroup.cpp','Random.cpp','Timer.cpp','Int.cpp','IntMod.cpp',
       'Point.cpp','SECP256K1.cpp','hash/ripemd160.cpp','hash/sha256.cpp','hash/sha512.cpp',
       'hash/ripemd160_sse.cpp','hash/sha256_sse.cpp','Bech32.cpp']
BASE = 0x123456789000000000  # synthetic known keys, outside Puzzle 71
REVISION = '20261025-2'


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024), b''): h.update(chunk)
    return h.hexdigest()


def add(a,b):
    if a is None: return b
    if b is None: return a
    x,y=a; u,v=b
    if x==u:
        if (y+v)%P==0: return None
        m=3*x*x*pow(2*y,-1,P)%P
    else: m=(v-y)*pow((u-x)%P,-1,P)%P
    z=(m*m-x-u)%P
    return z,(m*(x-z)-y)%P


def key_hash(k):
    if not 0<k<N: raise ValueError('Invalid reference key')
    r=None; q=G
    while k:
        if k&1:r=add(r,q)
        q=add(q,q); k>>=1
    pub=bytes([2+(r[1]&1)])+r[0].to_bytes(32,'big')
    return hashlib.new('ripemd160',hashlib.sha256(pub).digest()).digest()


def address(h):
    payload=b'\0'+h
    raw=payload+hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]
    n=int.from_bytes(raw,'big');out=''
    while n:n,r=divmod(n,58);out=ALPHABET[r]+out
    return '1'*(len(raw)-len(raw.lstrip(b'\0')))+out


def decode_wif(s):
    n=0
    for c in s:n=n*58+ALPHABET.index(c)
    b=n.to_bytes((n.bit_length()+7)//8,'big')
    if len(b)!=38 or b[0]!=128 or b[-5]!=1:raise ValueError('Not compressed WIF')
    if hashlib.sha256(hashlib.sha256(b[:-4]).digest()).digest()[:4]!=b[-4:]:raise ValueError('WIF checksum')
    return int.from_bytes(b[1:33],'big')


def sentinel_keys(start,count,blocks):
    threads=blocks*256
    if count%(threads*1024):raise ValueError('Unaligned test')
    per=count//threads
    offsets={0,1,511,512,513,1023,1024,count//2,count-2,count-1}
    for tid in (0,1,threads//2,threads-1):
        for offset in (0,per-1): offsets.add(tid*per+offset)
    # Deterministic interior sentinels, generated from this fixture only.
    for i in range(8):offsets.add(int.from_bytes(hashlib.sha256(f'{start}:{count}:{i}'.encode()).digest(),'big')%count)
    inside={start+x for x in offsets if 0<=x<count}
    outside={start-1,start+count,start+count+1}
    return inside,outside


def correctness_batches(inside,outside):
    # The bundled BitCrack's >16-target Bloom path is outside this single-target
    # benchmark. Keep every sentinel and every outside guard, in separate runs.
    capacity=16-len(outside)
    if capacity<=0 or not outside or inside&outside:
        raise ValueError('Invalid correctness target batches')
    keys=sorted(inside)
    if not keys:raise ValueError('Missing correctness sentinels')
    return [(set(keys[i:i+capacity]),set(outside)) for i in range(0,len(keys),capacity)]


def parse_results(engine,text):
    if engine=='vanity':
        found=set()
        for k,h in re.findall(r'^TEST_FOUND ([0-9A-Fa-f]+) ([0-9a-f]{40})$',text,re.M):
            key=int(k,16)
            if key_hash(key).hex()!=h:raise RuntimeError('Vanity result failed independent HASH160 check')
            if key in found:raise RuntimeError('Duplicate Vanity result')
            found.add(key)
        return found
    found=set()
    for token in re.findall(r'(?<![1-9A-HJ-NP-Za-km-z])[KL][1-9A-HJ-NP-Za-km-z]{51}(?![1-9A-HJ-NP-Za-km-z])',text):
        found.add(decode_wif(token))
    # Legacy hex records, if present. Never interpret arbitrary log numbers as keys.
    for k in re.findall(r'(?:Priv\s*\(HEX\)|Private\s*Key|PrivateKey)\s*[:=]\s*(?:0x)?([0-9A-Fa-f]{1,64})\b',text,re.I):
        found.add(int(k,16))
    return found


def validate_result(engine,log,result,count,expected):
    if re.search(r'\b(error|failed|items lost)\b',log,re.I):raise RuntimeError('Engine reported an error')
    if engine=='vanity':
        records=re.findall(r'^OFFLINE_DONE keys=(\d+) seconds=([0-9.e+-]+) mkeys=([0-9.e+-]+)$',log,re.M)
        if len(records)!=1 or int(records[0][0])!=count:raise RuntimeError('Missing/inexact completed key count')
    elif 'Reached end of keyspace' not in log:raise RuntimeError('BitCrack did not confirm range completion')
    actual=parse_results(engine,log if engine=='vanity' else result)
    if actual!=set(expected):raise RuntimeError(f'Synthetic key recovery mismatch: expected {len(expected)}, got {len(actual)}')


def write_json(p,data):p.write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n')


def run_capture(cmd,cwd=None,env=None,timeout=30):
    r=subprocess.run([str(x) for x in cmd],cwd=cwd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=timeout)
    if r.returncode:raise RuntimeError(f'Command failed ({r.returncode}): {cmd[0]}\n{r.stdout[-1800:]}')
    return r.stdout


def verify_bundle():
    manifest=json.loads((ROOT/'FILES.json').read_text())
    for rel,expected in manifest.items():
        p=ROOT/rel
        if Path(rel).is_absolute() or '..' in Path(rel).parts or p.is_symlink() or not p.is_file() or sha(p)!=expected:
            raise RuntimeError(f'Kit checksum mismatch: {rel}')


def smi(fields,uuid=None):
    cmd=['nvidia-smi','--query-gpu='+','.join(fields),'--format=csv,noheader,nounits']
    if uuid:cmd+=['-i',uuid]
    return [[x.strip() for x in row] for row in csv.reader(run_capture(cmd).splitlines())]


def compute_pids(uuid):
    text=run_capture(['nvidia-smi','--query-compute-apps=gpu_uuid,pid','--format=csv,noheader,nounits'])
    return {int(row[1].strip()) for row in csv.reader(text.splitlines()) if len(row)==2 and row[0].strip()==uuid}


def telemetry(uuid):
    rows=smi(['uuid','temperature.gpu','power.draw','power.limit','utilization.gpu','clocks.sm','clocks.mem','fan.speed'],uuid)
    if len(rows)!=1 or rows[0][0]!=uuid:raise RuntimeError('Physical GPU identity changed')
    names=['temperature_c','power_w','power_limit_w','utilization_percent','sm_clock_mhz','memory_clock_mhz','fan_percent']
    values={}
    for name,x in zip(names,rows[0][1:]):
        try:values[name]=float(x)
        except ValueError:values[name]=None
    if values['temperature_c'] is None or values['power_limit_w'] is None:raise RuntimeError('Required GPU telemetry unavailable')
    return values


def require_idle(uuid):
    # Driver accounting can briefly retain a process or utilization after exit.
    deadline=time.monotonic()+15
    while True:
        busy=compute_pids(uuid)
        utilization=telemetry(uuid)['utilization_percent']
        if not busy and utilization is not None and utilization<=10:return
        if time.monotonic()>=deadline:
            raise RuntimeError('GPU ocupada; para normalmente as pesquisas antes de medir. PIDs: '+str(sorted(busy)))
        time.sleep(1)


def stop_owned(p):
    if p.poll() is not None:return
    try:os.killpg(p.pid,signal.SIGTERM)
    except ProcessLookupError:return
    try:p.wait(timeout=5)
    except subprocess.TimeoutExpired:
        try:os.killpg(p.pid,signal.SIGKILL)
        except ProcessLookupError:pass
        p.wait(timeout=5)


def process(cmd,directory,env,uuid=None,power_limit=None,timeout=900):
    samples=[]; log=directory/'engine.log';start=time.monotonic(); next_sample=start;last_status=start
    with log.open('w') as f:
        p=subprocess.Popen([str(x) for x in cmd],cwd=directory,env=env,stdout=f,stderr=subprocess.STDOUT,start_new_session=True)
        try:
            while p.poll() is None:
                now=time.monotonic()
                if now-start>timeout:raise RuntimeError('Owned command timeout')
                if uuid and now>=next_sample:
                    t=telemetry(uuid);t['elapsed_seconds']=now-start;samples.append(t);next_sample=now+1
                    if t['temperature_c']>=85:raise RuntimeError('Teste interrompido: GPU atingiu 85 °C.')
                    if abs(t['power_limit_w']-power_limit)>0.5:raise RuntimeError('Limite de potência mudou; comparação interrompida.')
                    if compute_pids(uuid)-{p.pid}:raise RuntimeError('Outro processo começou a usar a GPU; comparação interrompida.')
                if now-last_status>=20:
                    print(f'  A executar {directory.name}: {int(now-start)} s',flush=True);last_status=now
                time.sleep(.1)
            if p.returncode:raise RuntimeError(f'Comando terminou com código {p.returncode}. Consulta {log}')
        finally:
            stop_owned(p)
            if uuid:write_json(directory/'telemetry.json',samples)
    return time.monotonic()-start,samples


def find_bitcrack(explicit):
    paths=[Path(explicit)] if explicit else [Path('/usr/libexec/OpenPuzzle/cuBitCrack'),Path('/usr/local/libexec/OpenPuzzle/cuBitCrack')]
    if not explicit:
        candidate=shutil.which('cuBitCrack')
        if candidate:paths.append(Path(candidate))
    for p in paths:
        if p.is_file() and os.access(p,os.X_OK):return p.resolve()
    raise RuntimeError('cuBitCrack instalado não encontrado. Usa --bitcrack CAMINHO para indicar o motor do OpenPuzzle.')


def correctness_batches(inside,outside):
    # The bundled BitCrack's >16-target Bloom path is outside this single-target
    # benchmark. Keep every sentinel and every outside guard, in separate runs.
    capacity=16-len(outside)
    if capacity<=0 or not outside or inside&outside:
        raise ValueError('Invalid correctness target batches')
    keys=sorted(inside)
    if not keys:raise ValueError('Missing correctness sentinels')
    return [(set(keys[i:i+capacity]),set(outside)) for i in range(0,len(keys),capacity)]


def parse_results(engine,text):
    if engine=='vanity':
        found=set()
        for k,h in re.findall(r'^TEST_FOUND ([0-9A-Fa-f]+) ([0-9a-f]{40})$',text,re.M):
            key=int(k,16)
            if key_hash(key).hex()!=h:raise RuntimeError('Vanity result failed independent HASH160 check')
            if key in found:raise RuntimeError('Duplicate Vanity result')
            found.add(key)
        return found
    found=set()
    for token in re.findall(r'(?<![1-9A-HJ-NP-Za-km-z])[KL][1-9A-HJ-NP-Za-km-z]{51}(?![1-9A-HJ-NP-Za-km-z])',text):
        found.add(decode_wif(token))
    # Legacy hex records, if present. Never interpret arbitrary log numbers as keys.
    for k in re.findall(r'(?:Priv\s*\(HEX\)|Private\s*Key|PrivateKey)\s*[:=]\s*(?:0x)?([0-9A-Fa-f]{1,64})\b',text,re.I):
        found.add(int(k,16))
    return found


def validate_result(engine,log,result,count,expected):
    if re.search(r'\b(error|failed|items lost)\b',log,re.I):raise RuntimeError('Engine reported an error')
    if engine=='vanity':
        records=re.findall(r'^OFFLINE_DONE keys=(\d+) seconds=([0-9.e+-]+) mkeys=([0-9.e+-]+)$',log,re.M)
        if len(records)!=1 or int(records[0][0])!=count:raise RuntimeError('Missing/inexact completed key count')
    elif 'Reached end of keyspace' not in log:raise RuntimeError('BitCrack did not confirm range completion')
    actual=parse_results(engine,log if engine=='vanity' else result)
    if actual!=set(expected):raise RuntimeError(f'Synthetic key recovery mismatch: expected {len(expected)}, got {len(actual)}')
