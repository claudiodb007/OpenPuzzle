"""Bind an existing validated measurement to the ordinary CLI; no GPU/HTTP/measurement."""
import hashlib,os,stat
from pathlib import Path
import Profile
import Evidence as e
import Support as s

def bind(store,cli,adapter,destination):
    store=Path(store);destination=Path(destination)
    record,measurement,paths=Profile.read_selected(store)
    cli=Profile.file(cli);adapter=Profile.file(adapter)
    e.require(os.access(cli,os.X_OK) and os.access(adapter,os.X_OK),'New CLI/adapter must be executable')
    proof=Path(record['evidence']['path'])
    files=[store/'selected.json',store/(record['binding_sha256']+'.json'),proof,
           *paths.values(),Path(measurement['source_checksums']['path']),Path(measurement['prior_report'])/'SUMMARY.json']
    files += [proof.parent/row['label']/'result.json' for row in measurement['measurements']]
    files=list(dict.fromkeys(p.resolve() for p in files))
    e.require(1<=len(files)<=32,'Unexpected measurement evidence count')
    def quote(value):
        value=str(value);e.require(not any(c in value for c in '\n\r\0'),'Unrepresentable bound profile value')
        return '"'+value.replace('\\','\\\\').replace('"','\\"')+'"'
    text=('OPV-RUN-PROFILE-1\n'+s.sha(cli)+' local-only\n'+quote(paths['engine'].resolve())+' '+quote(adapter.resolve())+' '+s.sha(paths['engine'])+' '+s.sha(adapter)+'\n'+
          quote(record['uuid'])+' '+quote(record['gpu'])+' '+quote(record['driver'])+' '+str(record['grid'])+' '+repr(record['planning_mkeys'])+' '+repr(record['power_limit_w'])+'\n'+str(len(files))+'\n'+
          ''.join(quote(p)+' '+s.sha(Profile.file(p))+'\n' for p in files))
    e.require(destination.is_absolute() and destination.parent.resolve()==destination.parent and not destination.is_symlink(),'Unsafe profile destination')
    if destination.exists():
        info=destination.stat();e.require(stat.S_ISREG(info.st_mode) and info.st_uid==os.getuid() and not info.st_mode&0o077 and destination.read_text()==text,'Existing bound profile differs; preserved')
    else:
        pending=destination.with_name(destination.name+'.pending')
        fd=os.open(pending,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
        with os.fdopen(fd,'w') as out:out.write(text);out.flush();os.fsync(out.fileno())
        os.replace(pending,destination)
        fd=os.open(destination.parent,os.O_DIRECTORY);os.fsync(fd);os.close(fd)
    return dict(path=str(destination),sha256=s.sha(destination),evidence_sha256={str(p):s.sha(p) for p in files},production_enabled=False)
