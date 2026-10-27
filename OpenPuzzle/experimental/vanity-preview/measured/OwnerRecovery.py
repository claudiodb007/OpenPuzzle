"""GPL-3.0-only. Read-only process identity and durable peer-journal gates.

No process is stopped, no checkpoint edited, and no claim or completion sent.
"""
import json
import os
import re
from pathlib import Path
import Evidence as e
import Resume as r
import Support as s


def _read(proc_pid):
    boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip()
    root=Path('/proc',str(proc_pid))
    text=(root/'stat').read_text()
    fields=text.rsplit(')',1)[1].split()
    e.require(len(fields)>=20 and int(fields[19])>0,'Incomplete owned process identity')
    if fields[0] in ('Z','X'):return None,fields[0]
    rows=(root/'status').read_text().splitlines()
    ids=next((line.split()[1:] for line in rows if line.startswith('NSpid:')),None)
    e.require(ids,'PID namespace identity unavailable')
    return dict(pid=int(ids[-1]),proc_pid=int(proc_pid),start_ticks=int(fields[19]),boot_id=boot,
                pid_namespace=os.readlink(root/'ns/pid'),proc_device=os.stat('/proc').st_dev),fields[0]


def capture(pid):
    e.require(type(pid) is int and pid>1,'Invalid owned process identity')
    if hasattr(os,'pidfd_open'):
        fd=os.pidfd_open(pid,0)
        try:
            rows=Path('/proc/self/fdinfo',str(fd)).read_text().splitlines()
            proc_pid=int(next(line.split()[1] for line in rows if line.startswith('Pid:')))
            if proc_pid<=1:raise ProcessLookupError('Owned child already exited')
            identity,state=_read(proc_pid)
        finally:os.close(fd)
    else:
        # A numeric /proc fallback is valid only when this mount uses our PIDs.
        e.require(int(Path('/proc/self/stat').read_text().split(' ',1)[0])==os.getpid(),
                  'PID namespace requires pidfd identity support')
        identity,state=_read(pid)
    if identity is None:raise ProcessLookupError('Owned child already exited')
    e.require(identity['pid']==pid,'Owned process identity unavailable')
    return identity


def dead(identity):
    e.require(isinstance(identity,dict) and set(identity)=={'pid','proc_pid','start_ticks','boot_id','pid_namespace','proc_device'}
              and type(identity['pid']) is int and identity['pid']>1
              and type(identity['proc_pid']) is int and identity['proc_pid']>1
              and type(identity['proc_device']) is int and identity['proc_device']>0
              and type(identity['start_ticks']) is int and identity['start_ticks']>0
              and re.fullmatch(r'pid:\[[0-9]+\]',identity['pid_namespace'])
              and re.fullmatch(r'[0-9a-f-]{36}',identity['boot_id']), 'Unknown process identity; review required')
    if Path('/proc/sys/kernel/random/boot_id').read_text().strip()!=identity['boot_id']:return True
    e.require(os.stat('/proc').st_dev==identity['proc_device'],'Process identity belongs to another proc view; review required')
    try:current,state=_read(identity['proc_pid'])
    except (FileNotFoundError,ProcessLookupError):return True
    return current!=identity or state in ('Z','X')


def peer(directory,meta):
    active=e.read_json(r.private_file(directory,'coordinator-active.json'))
    e.require(active.get('schema')=='openpuzzle_owned_transaction_v1'
              and not active.get('review_required',False),'Coordinator transaction requires review')
    e.require(dead(active['owner']) and dead(active['native']),'Previous coordinator owner is still active or unknown')
    path=r.private_file(directory,'http-live.json')
    e.require(s.sha(path)==active['journal_sha256'],'Live HTTP journal changed; review required')
    live=e.read_json(path)
    expected=dict(metadata_sha256=s.sha(r.private_file(directory,'coordinator-metadata.json')),
                  client_id=meta['client_id'],server=meta['native_options']['--server'])
    e.require(live['schema']=='openpuzzle_http_live_v1' and live['identity']==expected
              and live['owner_token']==active['owner_token'] and live['pending'] is None,
              'Incomplete or ambiguous live HTTP transaction')
    e.require(len(live['requests'])==len(live['responses']),'Incomplete live HTTP responses')
    return active,live


def token(value):
    import hashlib
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()
