"""GPL-3.0-only. Single-assignment loopback runtime for the local Vanity preview.

This module owns the local HTTP contract and explicit native command binding.
It does not import test code, discover GPUs, launch kernels or contact production.
Fault injection and synthetic engines belong to the tests.
"""
import hashlib
import http.server
import json
import math
import os
from pathlib import Path
import re
import tempfile
import threading
import uuid

START = 0x123456789000000000
ASSIGNMENT = 'coordinator-assignment-A'


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for part in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(part)
    return h.hexdigest()


def private_write(path, data):
    path=Path(path)
    require(path.is_absolute() and path.resolve()==path and path.parent.is_dir(), 'Unsafe live journal path')
    fd,name=tempfile.mkstemp(prefix='http-live-',dir=path.parent)
    try:
        with os.fdopen(fd,'wb') as stream:
            stream.write(data);stream.flush();os.fsync(stream.fileno())
        os.replace(name,path)
        directory=os.open(path.parent,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(directory)
        finally:os.close(directory)
    finally:
        if os.path.exists(name):os.unlink(name)


class LocalAssignmentServer:
    """One local claim, or the same assignment on resume; never a next claim."""

    def __init__(self, root, *, cli, adapter, engine, count, grid, segment,
                 target, planning, gpu_uuid, port=0, client_id=None,
                 resume_only=False, initial_checked=0):
        self.root = Path(root)
        self.cli = Path(cli)
        self.adapter = Path(adapter)
        self.engine = Path(engine)
        for path in (self.root, self.cli, self.adapter, self.engine):
            require(path.is_absolute() and path.resolve() == path, 'Explicit canonical local path required')
        require(self.root.is_dir() and all(p.is_file() for p in (self.cli, self.adapter, self.engine)),
                'Local root/executable missing')
        require(all(type(n) is int and n > 0 for n in (count, grid, segment))
                and count <= 2**36, 'Invalid local assignment bounds')
        require(type(port) is int and 0 <= port < 65536, 'Invalid loopback port')
        require(type(resume_only) is bool and type(initial_checked) is int
                and 0 <= initial_checked <= count, 'Invalid local resume state')
        require(not resume_only or port > 0 and client_id is not None,
                'Resume requires its original port and client')
        require(resume_only or initial_checked == 0, 'Fresh assignment must start at zero')
        require(isinstance(gpu_uuid, str) and re.fullmatch(r'GPU-[0-9a-fA-F-]{36}', gpu_uuid),
                'Physical GPU UUID required')
        require(isinstance(target, str) and target and '\n' not in target and '\r' not in target,
                'Explicit local target required')
        self.planning = float(planning)
        require(math.isfinite(self.planning) and self.planning > 0, 'Invalid planning rate')
        self.client_id = client_id or 'opv-coord-' + str(uuid.uuid4())
        require(re.fullmatch(r'opv-coord-[A-Za-z0-9-]{1,80}', self.client_id), 'Invalid local client')
        self.gpu_uuid = gpu_uuid
        self.resume_only = resume_only
        self.count, self.grid, self.segment, self.target = count, grid, segment, target
        self.requests, self.responses, self.errors = [], [], []
        self.claims = self.complete_attempts = self.completions = self.reports = 0
        self.progress = [initial_checked] if resume_only else []
        self.seen = {}
        self._lock = threading.RLock()
        self._closed = False
        self._journal = None
        self.base_requests, self.base_responses = [], []
        outer = self

        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = 'HTTP/1.1'

            def log_message(self, *_):
                pass

            def do_POST(self):
                self.connection.settimeout(5)
                try:
                    size = int(self.headers['Content-Length'])
                    require(0 < size <= 16384, 'Invalid local request size')
                    body = json.loads(self.rfile.read(size))
                    require(isinstance(body, dict), 'Local JSON object required')
                    with outer._lock:
                        outer.requests.append(dict(path=self.path, payload=body))
                        outer.persist(dict(path=self.path,payload=body))
                        before = (outer.claims, len(outer.progress), outer.completions, outer.reports)
                        code, response = outer.handle(self.path, body)
                        after = (outer.claims, len(outer.progress), outer.completions, outer.reports)
                        applied = dict(zip(('claim', 'progress', 'complete', 'solution'),
                                           (b > a for a, b in zip(before, after))))
                        outer.responses.append(dict(path=self.path, payload=body, status=code,
                                                    reply=response, applied=applied))
                        outer.persist(None)
                except Exception as error:
                    with outer._lock:
                        outer.errors.append(str(error))
                    code, response = 400, dict(error='local_contract_refused')
                outer.respond(self, code, response)

        self.server = http.server.ThreadingHTTPServer(('127.0.0.1', port), Handler)
        self.server.daemon_threads = True
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        try:
            self.thread.start()
        except BaseException:
            self.server.server_close()
            raise
        self.url = 'http://127.0.0.1:' + str(self.server.server_port)

    def configure_journal(self, path, identity, owner_token, requests, responses, on_persist):
        with self._lock:
            require(not self.requests and self._journal is None, 'Live journal configured after requests')
            require(len(requests)==len(responses), 'Incomplete previous HTTP journal')
            self.base_requests, self.base_responses = list(requests), list(responses)
            self._journal=(Path(path),identity,owner_token,on_persist)
            self.persist(None)

    def persist(self,pending):
        if self._journal is None:return
        path,identity,token,callback=self._journal
        value=dict(schema='openpuzzle_http_live_v1',identity=identity,owner_token=token,
                   requests=self.base_requests+self.requests,responses=self.base_responses+self.responses,pending=pending)
        data=(json.dumps(value,sort_keys=True,indent=2)+'\n').encode()
        private_write(path,data)
        callback(hashlib.sha256(data).hexdigest())

    def close(self):
        if not self._closed:
            self.server.shutdown()
            self.server.server_close()
            self.thread.join()
            self._closed = True

    def respond(self, handler, code, response):
        data = json.dumps(response, separators=(',', ':')).encode()
        handler.send_response(code)
        handler.send_header('Content-Length', str(len(data)))
        handler.send_header('Connection', 'close')
        handler.end_headers()
        handler.wfile.write(data)
        handler.close_connection = True

    def assignment_identity(self, body):
        require(body.get('client_id') == self.client_id and body.get('assignment_id') == ASSIGNMENT,
                'Local client/assignment differs')

    def handle(self, path, body):
        self.seen[path] = self.seen.get(path, 0) + 1
        if path == '/api/client/register':
            require(not self.resume_only and self.seen[path] == 1
                    and body == dict(client_id=self.client_id), 'Invalid/repeated local registration')
            return 200, dict(success=True)
        if path == '/api/client/heartbeat':
            require(body.get('client_id') == self.client_id and body.get('engines') ==
                    [dict(name='Vanity', backend='cuda', installed=True, available=True)],
                    'Local heartbeat differs')
            return 200, dict(success=True)
        if path == '/api/range/claim':
            require(not self.resume_only and self.claims == 0 and self.completions == 0,
                    'Another local claim is not allowed')
            require(body.get('client_id') == self.client_id and body.get('puzzle') == 20
                    and body.get('backend') == 'cuda' and body.get('target_duration_minutes') == 60
                    and abs(float(body.get('speed_mkeys', -1)) - self.planning) < .000001,
                    'Local claim metadata differs')
            self.claims += 1
            return 200, dict(assignment_id=ASSIGNMENT, puzzle=20, range_id=37, target=self.target,
                search_mode='linear', required_backend='cuda', start=f'{START:064x}',
                end=f'{START + self.count - 1:064x}')
        self.assignment_identity(body)
        require(self.resume_only or self.claims == 1, 'No local assignment claimed')
        if path == '/api/range/progress':
            n = int(body.get('keys_checked', -1))
            require(not self.completions and not self.reports and 0 <= n <= self.count
                    and (not self.progress or n >= self.progress[-1]), 'Local progress regressed/escaped')
            self.progress.append(n)
            return 200, dict(success=True)
        if path == '/api/range/complete':
            require(not self.completions and not self.reports and self.progress
                    and int(body.get('keys_checked', -1)) == self.count == self.progress[-1]
                    and body.get('status') == 'completed' and body.get('exit_code') == 0,
                    'Local completion lacks confirmed coverage')
            self.complete_attempts += 1
            self.completions += 1
            return 200, dict(success=True)
        if path == '/api/puzzle/report-solution':
            require(not self.completions and not self.reports and set(body) == {'assignment_id', 'client_id'},
                    'Invalid local solution notice')
            self.reports += 1
            return 200, dict(success=True)
        raise RuntimeError('Unsupported local endpoint')

    def options(self):
        # Preserve the argument order and native binding used by validated runs.
        return dict(zip(('--server', '--root', '--controls', '--engine', '--engine-sha256',
            '--adapter', '--adapter-sha256', '--client-id', '--target', '--puzzle', '--grid',
            '--segment-keys', '--scope-start', '--scope-end', '--gpu-uuid', '--speed-mkeys'),
            (self.url, str(self.root), str(self.root / 'controls'), str(self.engine), sha(self.engine),
             str(self.adapter), sha(self.adapter), self.client_id, self.target, '20', str(self.grid),
             str(self.segment), f'{START:064x}', f'{START + self.count - 1:064x}', self.gpu_uuid,
             format(self.planning, '.12f'))))

    def command(self, resume=False):
        command = [str(self.cli), 'run', '20', '--engine', 'vanity', '--backend', 'cuda',
                   '--vanity-native-local', '--enable-experimental-vanity-local']
        for key, value in self.options().items():
            if key != '--puzzle':
                command.extend(['--target-engine' if key == '--engine' else key, value])
        if resume:
            command.append('--resume')
        return command
