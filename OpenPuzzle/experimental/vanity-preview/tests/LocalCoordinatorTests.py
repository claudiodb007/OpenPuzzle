#!/usr/bin/env python3
"""Local runtime contract over real loopback HTTP; no GPU or engine execution."""
import ast
import http.client
import json
import os
import shutil
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import unittest

MEASURED = Path(__file__).resolve().parent.parent / 'measured'
sys.path.insert(0, str(MEASURED))
import LocalCoordinator as local


class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.binary = self.root / 'explicit executable'
        self.binary.write_text('synthetic identity only; never executed')
        self.args = dict(cli=self.binary, adapter=self.binary, engine=self.binary,
            count=100, grid=1, segment=50, target='SyntheticLocalTarget', planning=2519.41,
            gpu_uuid='GPU-11111111-2222-3333-4444-555555555555')
        self.servers = []

    def tearDown(self):
        for server in self.servers:
            server.close()
        self.tmp.cleanup()

    def server(self, **kwargs):
        server = local.LocalAssignmentServer(self.root, **(self.args | kwargs))
        self.servers.append(server)
        return server

    def post(self, server, path, body, expected=200):
        connection = http.client.HTTPConnection('127.0.0.1', server.server.server_port, timeout=5)
        try:
            connection.request('POST', path, json.dumps(body), {'Content-Type': 'application/json'})
            response = connection.getresponse()
            result = json.loads(response.read())
            self.assertEqual(response.status, expected, result)
            return result
        finally:
            connection.close()

    def claim(self, server):
        return self.post(server, '/api/range/claim', dict(client_id=server.client_id, puzzle=20,
            backend='cuda', target_duration_minutes=60, speed_mkeys=server.planning))

    def assignment(self, server, **kwargs):
        return dict(client_id=server.client_id, assignment_id=local.ASSIGNMENT, **kwargs)

    def test_single_assignment_wire_contract(self):
        server = self.server()
        self.assertEqual(server.server.server_address[0], '127.0.0.1')
        self.post(server, '/api/client/register', dict(client_id=server.client_id))
        self.post(server, '/api/client/heartbeat', dict(client_id=server.client_id,
            engines=[dict(name='Vanity', backend='cuda', installed=True, available=True)]))
        assignment = self.claim(server)
        self.assertEqual(assignment['start'], f'{local.START:064x}')
        self.assertEqual(assignment['end'], f'{local.START + 99:064x}')
        for n in (0, 20, 100):
            self.post(server, '/api/range/progress', self.assignment(server, keys_checked=str(n)))
        self.post(server, '/api/range/complete', self.assignment(server, keys_checked='100', status='completed', exit_code=0))
        self.assertEqual((server.claims, server.completions, server.errors), (1, 1, []))
        self.assertEqual(len(server.requests), 7)

    def test_another_claim_and_early_completion_refused(self):
        server = self.server()
        self.claim(server)
        self.post(server, '/api/range/claim', dict(client_id=server.client_id), 400)
        self.post(server, '/api/range/complete', self.assignment(server, keys_checked='100', status='completed', exit_code=0), 400)
        self.assertEqual((server.claims, server.completions), (1, 0))

    def test_resume_uses_same_port_client_and_blocks_claim(self):
        previous = self.server()
        port = previous.server.server_port
        client = previous.client_id
        previous.close()
        server = self.server(port=port, client_id=client, resume_only=True, initial_checked=40)
        self.assertEqual(server.url, previous.url)
        self.post(server, '/api/range/claim', dict(client_id=client), 400)
        self.post(server, '/api/range/progress', self.assignment(server, keys_checked='39'), 400)
        self.post(server, '/api/range/progress', self.assignment(server, keys_checked='100'))
        self.post(server, '/api/range/complete', self.assignment(server, keys_checked='100', status='completed', exit_code=0))
        self.assertEqual((server.claims, server.completions), (0, 1))

    def test_wrong_identity_and_regression_leave_progress_unchanged(self):
        server = self.server()
        self.claim(server)
        self.post(server, '/api/range/progress', self.assignment(server, keys_checked='50'))
        for body in (dict(client_id='other', assignment_id=local.ASSIGNMENT, keys_checked='80'),
                     dict(client_id=server.client_id, assignment_id='other', keys_checked='80'),
                     self.assignment(server, keys_checked='49'), self.assignment(server, keys_checked='101')):
            self.post(server, '/api/range/progress', body, 400)
        self.assertEqual(server.progress, [50])

    def test_duplicate_completion_and_progress_after_completion_refused(self):
        server = self.server()
        self.claim(server)
        self.post(server, '/api/range/progress', self.assignment(server, keys_checked='100'))
        body = self.assignment(server, keys_checked='100', status='completed', exit_code=0)
        self.post(server, '/api/range/complete', body)
        self.post(server, '/api/range/complete', body, 400)
        self.post(server, '/api/range/progress', self.assignment(server, keys_checked='100'), 400)
        self.assertEqual(server.completions, 1)

    def test_only_solution_notice_never_private_material(self):
        server = self.server()
        self.claim(server)
        self.post(server, '/api/puzzle/report-solution', self.assignment(server, private_key_wif='synthetic'), 400)
        self.post(server, '/api/puzzle/report-solution', self.assignment(server))
        self.assertEqual(server.reports, 1)
        self.post(server, '/api/range/complete', self.assignment(server, keys_checked='100', status='completed', exit_code=0), 400)

    def test_explicit_native_command_preserves_options(self):
        server = self.server()
        command = server.command()
        self.assertEqual(command[:9], [str(self.binary), 'run', '20', '--engine', 'vanity', '--backend', 'cuda', '--vanity-native-local', '--enable-experimental-vanity-local'])
        self.assertEqual(command[command.index('--target-engine') + 1], str(self.binary))
        self.assertEqual(command[command.index('--gpu-uuid') + 1], self.args['gpu_uuid'])
        self.assertEqual(command[command.index('--engine-sha256') + 1], local.sha(self.binary))
        self.assertEqual(server.command(True), command + ['--resume'])

    def test_missing_binding_and_invalid_resume_refused_before_server(self):
        for kwargs in (dict(gpu_uuid=''), dict(planning=float('nan')), dict(count=0),
                       dict(port=-1), dict(resume_only=True), dict(initial_checked=1)):
            with self.assertRaises(RuntimeError):
                self.server(**kwargs)
        self.assertEqual(sorted(p.name for p in self.root.iterdir()), [self.binary.name])

    def test_busy_original_port_preserves_files(self):
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            sock.listen()
            with self.assertRaises(OSError):
                self.server(port=sock.getsockname()[1], client_id='opv-coord-resume', resume_only=True)
        self.assertEqual(sorted(p.name for p in self.root.iterdir()), [self.binary.name])

    def test_runtime_import_has_no_test_or_argv_dependency(self):
        source = (MEASURED / 'Coordinator.py').read_text()
        self.assertNotIn('CoordinatorSessionTests', source)
        self.assertNotIn('fixture_module', source)
        tree = ast.parse((MEASURED / 'LocalCoordinator.py').read_text())
        modules = [node.module for node in ast.walk(tree) if isinstance(node, ast.ImportFrom)]
        modules += [alias.name for node in ast.walk(tree) if isinstance(node, ast.Import) for alias in node.names]
        self.assertFalse(any(name and ('tests' in name or 'unittest' in name) for name in modules))
        code = '''import json,sys,threading
from pathlib import Path
sys.path.insert(0,sys.argv[1]);before=sys.argv[:];n=threading.active_count()
import LocalCoordinator
if sys.argv!=before or threading.active_count()!=n:raise RuntimeError('Import changed process state')
args=json.loads(sys.argv[3]);args.update({k:Path(args[k]) for k in ('cli','adapter','engine')})
server=LocalCoordinator.LocalAssignmentServer(Path(sys.argv[2]),**args)
try:
 body=dict(client_id=server.client_id,puzzle=20,backend='cuda',target_duration_minutes=60,speed_mkeys=server.planning)
 server.handle('/api/range/claim',body)
 try:server.handle('/api/range/claim',body)
 except RuntimeError:pass
 else:raise RuntimeError('Optimized runtime accepted another claim')
finally:server.close()
'''
        args={key:str(value) if isinstance(value,Path) else value for key,value in self.args.items()}
        standalone=self.root/'standalone';standalone.mkdir()
        shutil.copyfile(MEASURED/'LocalCoordinator.py',standalone/'LocalCoordinator.py')
        subprocess.run([sys.executable, '-B', '-I', '-O', '-c', code, str(standalone), str(self.root), json.dumps(args)], check=True)
        self.assertFalse((standalone/'__pycache__').exists(), 'Isolated import wrote bytecode into its source tree')


if __name__ == '__main__':
    unittest.main(verbosity=2)
