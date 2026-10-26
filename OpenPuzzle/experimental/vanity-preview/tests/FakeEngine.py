#!/usr/bin/env python3
"""Synthetic protocol fixture only. No GPU or network."""
import ctypes,os,signal,sys,time
from pathlib import Path
if sys.argv[1:]==['--identity']:
    print('OpenPuzzle-VanityRange 0.1.0 CUDA synthetic protocol 1');sys.exit()
_,gate,start,count,grid,keys,output,mode,owner=sys.argv
assert gate=='--synthetic-only'
def stopped_by_owner(*_):
    # Explicit fixture witness also works when /proc uses a different PID namespace.
    Path(output).parent.joinpath('engine-stopped.txt').write_text('SIGTERM\n')
    sys.exit(130)
signal.signal(signal.SIGTERM,stopped_by_owner)
ctypes.CDLL(None).prctl(1,signal.SIGTERM,0,0,0)
if os.getppid()!=int(owner):sys.exit(2)
count=int(count);batch=int(grid)*256*1024
Path(output).parent.joinpath('engine.pid').write_text(str(os.getpid()))
f=Path(output).open('x');os.chmod(output,0o600)
behavior=os.getenv('OPV_FAKE_MODE','normal')
driver=behavior=='driver'
if driver:
    sys.path.insert(0,os.environ['OPV_TEST_LIBRARY'])
    from BuildSupport import key_hash
    target_keys=[int(k,16) for k in Path(keys).read_text().split()]
    first=int(start,16)
print('OPV1 BEGIN',start,count,batch,flush=True)
checked=0
while checked<count:
    prior=checked
    checked+=min(batch,count-checked)
    if behavior=='forged':print('OPV1 PROGRESS',checked+1,flush=True);time.sleep(.2);sys.exit(0)
    matched=False
    if driver:
        for key in target_keys:
            if first+prior<=key<first+checked:
                f.write(f'SYNTHETIC {key:064x} {key_hash(key).hex()}\n');matched=True
        f.flush();os.fsync(f.fileno())
    elif mode=='stop':f.write('SYNTHETIC '+start+' '+'0'*40+'\n');f.flush();os.fsync(f.fileno());matched=True
    print('OPV1 PROGRESS',checked,flush=True)
    if mode=='stop' and matched:print('OPV1 SOLUTION',checked,flush=True);sys.exit(10)
    if behavior=='slow' or driver:time.sleep(.08)
if behavior=='missing':sys.exit(0)
if driver and count<=65536:print('OPV1 TRACE',count,flush=True)
print('OPV1 COMPLETE',checked,flush=True)
sys.exit(3 if behavior=='badexit' else 0)
