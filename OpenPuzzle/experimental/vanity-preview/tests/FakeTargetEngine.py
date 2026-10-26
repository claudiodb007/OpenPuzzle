#!/usr/bin/env python3
"""Protocol fixture. Deliberately only simulates two fixed synthetic scalars."""
import ctypes,hashlib,os,signal,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent/'support'))
import Support as s
from LocalScenarios import wif
def targets(path):
    lines=Path(path).read_text().splitlines();assert lines[0]=='OPV-TARGETS-1'
    hashes=set()
    for line in lines[1:]:
        kind,value=line.split()
        if kind=='hash160':h=bytes.fromhex(value);assert len(h)==20
        else:
            assert kind=='p2pkh';n=0
            for c in value:n=n*58+s.ALPHABET.index(c)
            raw=b'\0'*(len(value)-len(value.lstrip('1')))+n.to_bytes((n.bit_length()+7)//8,'big')
            assert len(raw)==25 and raw[0]==0 and hashlib.sha256(hashlib.sha256(raw[:-4]).digest()).digest()[:4]==raw[-4:];h=raw[1:21]
        hashes.add(h)
    return hashes
if sys.argv[1:]==['--identity']:
    print('OpenPuzzle-VanityRange 0.2.0 CUDA target protocol 1');sys.exit()
if len(sys.argv)==3 and sys.argv[1]=='--target-info':
    for h in sorted(targets(sys.argv[2])):print(h.hex(),s.address(h))
    sys.exit()
_,gate,start,count,grid,target_file,output,mode,owner=sys.argv
assert gate=='--target-file' and mode in ('stop','exhaust')
ctypes.CDLL(None).prctl(1,signal.SIGTERM,0,0,0)
assert os.getppid()==int(owner)
stopped=False
def stop(*_):
    global stopped;stopped=True
signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
count=int(count);first=int(start,16);batch=int(grid)*256*1024;checked=0;hashes=targets(target_file)
f=Path(output).open('x');os.chmod(output,0o600)
print('OPV1 BEGIN',start,count,batch,flush=True)
while checked<count:
    prior=checked;checked+=min(batch,count-checked);found=False
    for key in [s.BASE,s.BASE-1]:
        h=s.key_hash(key)
        if first+prior<=key<first+checked and h in hashes:
            f.write(f'MATCH {key:064x} {h.hex()} {s.address(h)} {wif(key)}\n');f.flush();os.fsync(f.fileno());found=True
    print('OPV1 PROGRESS',checked,flush=True)
    if found and mode=='stop':print('OPV1 SOLUTION',checked,flush=True);sys.exit(10)
    time.sleep(.12)
    if stopped:print('OPV1 STOPPED',checked,flush=True);sys.exit(130)
if count<=65536:print('OPV1 TRACE',checked,flush=True)
print('OPV1 COMPLETE',checked,flush=True)
