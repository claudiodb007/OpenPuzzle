"""GPL-3.0-only. Isolated reference protocol, not a production server/client.

An SQLite transaction commits both an assignment effect and its immutable receipt.
No HTTP, GPU, scheduler, production database or existing profile is accessed.
"""
from contextlib import closing
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import time
import uuid

PROTOCOL = 'openpuzzle_vanity_receipts_v1'
KINDS = {'claim', 'progress', 'complete'}


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def encoded(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False)


def identifier(value):
    require(isinstance(value, str) and re.fullmatch(r'[A-Za-z0-9-]{1,100}', value), 'Invalid identity')
    return value


def operation_id(value):
    require(isinstance(value, str) and str(uuid.UUID(value)) == value, 'Canonical operation UUID required')
    return value


def count(value):
    require(isinstance(value, str) and re.fullmatch(r'0|[1-9][0-9]{0,20}', value), 'Canonical decimal count required')
    n = int(value)
    require(n <= 2**64-1, 'Count exceeds protocol bound')
    return n


def scope_digest(scope, total=None):
    require(type(scope) is dict and set(scope) == {'puzzle','range_id','start','end','target','required_backend'}, 'Exact assignment scope required')
    require(type(scope['puzzle']) is int and scope['puzzle'] == 20
            and type(scope['range_id']) is int and scope['range_id'] > 0
            and scope['required_backend'] == 'cuda', 'Local scope metadata differs')
    require(all(isinstance(scope[k],str) and re.fullmatch(r'[0-9a-f]{64}',scope[k]) for k in ('start','end')), 'Canonical range required')
    size = int(scope['end'],16)-int(scope['start'],16)+1
    require(0 < int(scope['start'],16) <= int(scope['end'],16) < 2**256
            and 0 < size <= 2**36 and (total is None or size == total), 'Range/total differs')
    require(isinstance(scope['target'],str) and re.fullmatch(r'[13][1-9A-HJ-NP-Za-km-z]{25,34}',scope['target']), 'Explicit public target required')
    return hashlib.sha256(encoded(scope).encode()).hexdigest()


def validate(request):
    require(type(request) is dict, 'Request object required')
    kind = request.get('kind')
    require(isinstance(kind, str) and kind in KINDS, 'Unsupported operation')
    fields = {'protocol', 'operation_id', 'client_id', 'kind', 'scope_sha256'}
    fields |= {'puzzle'} if kind == 'claim' else {'assignment_id', 'lease_epoch', 'keys_checked'}
    require(set(request) == fields and request['protocol'] == PROTOCOL, 'Request contract differs')
    operation_id(request['operation_id']); identifier(request['client_id'])
    require(isinstance(request['scope_sha256'],str) and re.fullmatch(r'[0-9a-f]{64}',request['scope_sha256']), 'Scope digest required')
    if kind == 'claim':
        require(type(request['puzzle']) is int and request['puzzle'] == 20, 'Isolated puzzle 20 required')
    else:
        operation_id(request['assignment_id'])
        require(type(request['lease_epoch']) is int and request['lease_epoch'] > 0, 'Lease epoch required')
        count(request['keys_checked'])
    return hashlib.sha256(encoded(request).encode()).hexdigest()


class Ledger:
    """Single isolated assignment; no production acquisition or scheduling policy."""

    def __init__(self, path, *, total, scope, lease_seconds=300, clock=lambda: int(time.time())):
        self.path = Path(path)
        require(self.path.is_absolute() and self.path.resolve() == self.path
                and self.path.parent.is_dir(), 'Canonical private ledger path required')
        info = self.path.parent.stat()
        require(info.st_uid == os.getuid() and info.st_mode & 0o077 == 0, 'Private ledger directory required')
        require(type(total) is int and 0 < total <= 2**36 and type(lease_seconds) is int
                and 1 <= lease_seconds <= 86400, 'Invalid local bounds')
        self.total, self.lease_seconds, self.clock = total, lease_seconds, clock
        self.scope = json.loads(encoded(scope))
        self.scope_sha256 = scope_digest(self.scope, total)
        existing = self.path.exists()
        if not existing:
            fd = os.open(self.path, os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW | os.O_WRONLY, 0o600)
            os.close(fd)
        require(self.path.is_file() and self.path.stat().st_uid == os.getuid()
                and self.path.stat().st_mode & 0o077 == 0, 'Unsafe ledger file')
        if existing:
            with closing(self.connect(readonly=True)) as db:
                require(db.execute('SELECT protocol,total,lease_seconds,scope FROM metadata').fetchall()
                        == [(PROTOCOL, str(total), lease_seconds, encoded(self.scope))], 'Ledger binding differs')
            return
        with closing(self.connect()) as db:
            db.executescript('''
              BEGIN IMMEDIATE;
              CREATE TABLE IF NOT EXISTS metadata (singleton INTEGER PRIMARY KEY CHECK(singleton=1), protocol TEXT NOT NULL, total TEXT NOT NULL, lease_seconds INTEGER NOT NULL, scope TEXT NOT NULL);
              CREATE TABLE IF NOT EXISTS assignment (singleton INTEGER PRIMARY KEY CHECK(singleton=1), id TEXT NOT NULL UNIQUE, client TEXT NOT NULL, epoch INTEGER NOT NULL, lease_until INTEGER NOT NULL, checked TEXT NOT NULL, status TEXT NOT NULL);
              CREATE TABLE IF NOT EXISTS receipts (operation_id TEXT PRIMARY KEY, client TEXT NOT NULL, digest TEXT NOT NULL, receipt TEXT NOT NULL);
              COMMIT;
            ''')
            db.execute('INSERT OR IGNORE INTO metadata VALUES (1,?,?,?,?)', (PROTOCOL, str(total), lease_seconds, encoded(self.scope)))
            require(db.execute('SELECT protocol,total,lease_seconds,scope FROM metadata').fetchall()
                    == [(PROTOCOL, str(total), lease_seconds, encoded(self.scope))], 'Ledger binding differs')
        directory = os.open(self.path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:os.fsync(directory)
        finally:os.close(directory)

    def connect(self, readonly=False):
        db = sqlite3.connect(self.path.as_uri() + ('?mode=ro' if readonly else '?mode=rw'),
                             uri=True, timeout=10, isolation_level=None)
        if not readonly:
            db.execute('PRAGMA journal_mode=DELETE')
            db.execute('PRAGMA synchronous=FULL')
        return db

    def execute(self, request, *, before_commit=None):
        # Detach caller-owned data before hashing/applying; no mutation can reuse
        # a digest for another effect while this call waits on the write lock.
        request = json.loads(encoded(request))
        digest = validate(request)
        require(request['scope_sha256'] == self.scope_sha256, 'Assignment scope differs')
        with closing(self.connect()) as db:
            db.execute('BEGIN IMMEDIATE')
            try:
                old = db.execute('SELECT client,digest,receipt FROM receipts WHERE operation_id=?',
                                 (request['operation_id'],)).fetchone()
                if old:
                    require(old[:2] == (request['client_id'], digest), 'Operation identity conflict')
                    db.commit()
                    return json.loads(old[2])
                now = self.clock()
                require(type(now) in (int, float) and 0 <= now < 2**53 and int(now) == now, 'Integer server UTC required')
                row = db.execute('SELECT id,client,epoch,lease_until,checked,status FROM assignment').fetchone()
                effect, reason = False, 'assignment_unavailable'
                kind = request['kind']
                if kind == 'claim':
                    if row is None:
                        row = (str(uuid.uuid4()), request['client_id'], 1, int(now)+self.lease_seconds, '0', 'assigned')
                        db.execute('INSERT INTO assignment VALUES (1,?,?,?,?,?,?)', row)
                        effect, reason = True, 'applied'
                else:
                    reason = 'assignment_not_found'
                    if row and row[0] == request['assignment_id'] and row[1] == request['client_id']:
                        reason = 'lease_refused'
                        if row[2] == request['lease_epoch'] and row[3] > now and row[5] == 'assigned':
                            n = count(request['keys_checked'])
                            reason = 'coverage_refused'
                            if int(row[4]) <= n <= self.total and (kind != 'complete' or n == self.total == int(row[4])):
                                effect, reason = True, 'applied'
                                row = (*row[:4], str(n), 'completed' if kind == 'complete' else 'assigned')
                                db.execute('UPDATE assignment SET checked=?, status=? WHERE singleton=1', row[4:])
                # A refused foreign request must not disclose another owner's state.
                visible = row if row and row[1] == request['client_id'] and (kind == 'claim' or row[0] == request['assignment_id']) else None
                receipt = dict(protocol=PROTOCOL, operation_id=request['operation_id'], request_sha256=digest,
                    client_id=request['client_id'], kind=kind, outcome='applied' if effect else 'refused',
                    reason=reason, assignment=self.public(visible), production_enabled=False)
                db.execute('INSERT INTO receipts VALUES (?,?,?,?)',
                           (request['operation_id'], request['client_id'], digest, encoded(receipt)))
                if before_commit is not None:before_commit()
                db.commit()
                return receipt
            except BaseException:
                db.rollback()
                raise

    def public(self, row):
        return None if row is None else dict(assignment_id=row[0], client_id=row[1], lease_epoch=row[2],
            lease_until=row[3], keys_checked=row[4], status=row[5], total=str(self.total), puzzle=20, scope=dict(self.scope))

    def query(self, request):
        """Read-only lookup of the exact original operation, never an assignment mutation."""
        digest = validate(request)
        require(request['scope_sha256'] == self.scope_sha256, 'Assignment scope differs')
        with closing(self.connect(readonly=True)) as db:
            row = db.execute('SELECT client,digest,receipt FROM receipts WHERE operation_id=?',
                             (request['operation_id'],)).fetchone()
            require(row is None or row[:2] == (request['client_id'], digest), 'Operation identity conflict')
            return dict(protocol=PROTOCOL, operation_id=request['operation_id'], request_sha256=digest,
                        client_id=request['client_id'], state='recorded' if row else 'unknown',
                        receipt=json.loads(row[2]) if row else None, production_enabled=False)


def reconcile(request, response):
    """Classify an exact receipt only. Does not launch, retry, seal or edit profiles.

    Existing owner/coverage/lease checks remain mandatory at future integration.
    A completion receipt confirms a server effect, never native coverage.
    """
    digest = validate(request)
    require(type(response) is dict and set(response) == {'protocol','operation_id','request_sha256',
            'client_id','state','receipt','production_enabled'}, 'Reconciliation response differs')
    require(all(response[k] == v for k,v in dict(protocol=PROTOCOL, operation_id=request['operation_id'],
            request_sha256=digest, client_id=request['client_id'], production_enabled=False).items())
            and response['production_enabled'] is False, 'Reconciliation identity differs')
    if response['state'] == 'unknown':
        require(response['receipt'] is None, 'Unknown operation includes receipt')
        return 'review_unknown'  # Never infer a new claim or authorize an automatic retry.
    require(response['state'] == 'recorded', 'Unknown reconciliation state')
    r = response['receipt']
    require(type(r) is dict and set(r) == {'protocol','operation_id','request_sha256','client_id','kind',
            'outcome','reason','assignment','production_enabled'}, 'Receipt shape differs')
    require(all(r[k] == response[k] for k in ('protocol','operation_id','request_sha256','client_id'))
            and r['kind'] == request['kind'] and r['production_enabled'] is False, 'Receipt identity differs')
    require(r['outcome'] in ('applied','refused'), 'Unknown receipt outcome')
    if r['outcome'] == 'refused':
        require(r['reason'] in ('assignment_unavailable','assignment_not_found','lease_refused','coverage_refused'), 'Unknown refusal')
        return 'blocked_refused'
    require(r['reason'] == 'applied', 'Applied receipt reason differs')
    a = r['assignment']
    require(type(a) is dict and set(a) == {'assignment_id','client_id','lease_epoch','lease_until','keys_checked','status','total','puzzle','scope'}, 'Assignment shape differs')
    operation_id(a['assignment_id'])
    require(a['client_id'] == request['client_id'] and type(a['puzzle']) is int and a['puzzle'] == 20
            and type(a['lease_epoch']) is int and a['lease_epoch'] > 0
            and type(a['lease_until']) is int and a['lease_until'] > 0
            and 0 <= count(a['keys_checked']) <= count(a['total']) and 0 < count(a['total']) <= 2**36, 'Assignment receipt differs')
    require(scope_digest(a['scope'],count(a['total'])) == request['scope_sha256'], 'Receipt scope differs')
    kind = request['kind']
    if kind == 'claim':
        require(a['status'] == 'assigned' and a['keys_checked'] == '0', 'Claim receipt differs')
        return 'claim_recorded'
    require(a['assignment_id'] == request['assignment_id'] and a['lease_epoch'] == request['lease_epoch']
            and a['keys_checked'] == request['keys_checked'], 'Operation assignment/count differs')
    require(a['status'] == ('completed' if kind == 'complete' else 'assigned'), 'Operation status differs')
    if kind == 'complete':require(a['keys_checked'] == a['total'], 'Completion total differs')
    return 'completion_recorded' if kind == 'complete' else 'progress_recorded'
