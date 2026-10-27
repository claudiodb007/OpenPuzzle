#!/usr/bin/env python3
"""Native recovery with real HTTP/adapter and synthetic inventory/engine only."""
import contextlib
import io
import json
import os
from pathlib import Path
import socket
import subprocess
import unittest
from unittest.mock import patch
import CoordinatorDesktopTests as desktop

c, s, e, CLI = desktop.c, desktop.s, desktop.e, desktop.base.CLI


class Tests(unittest.TestCase):
    setUp = desktop.Tests.setUp
    tearDown = desktop.Tests.tearDown
    hardware = desktop.Tests.hardware
    start = desktop.Tests.start
    resume = desktop.Tests.resume
    snapshot = desktop.Tests.snapshot

    @contextlib.contextmanager
    def fault(self, kind):
        parent = c.LocalAssignmentServer
        class Fault(parent):
            def handle(instance, path, body):
                if path == '/api/range/progress':
                    n = int(body['keys_checked'])
                    if kind == 'network-before' or kind == 'network-during' and n > 0:
                        return 503, dict(error='temporary_unavailable')
                    if kind == 'lease-before' or kind == 'lease-during' and n > 0:
                        return 409, dict(error='assignment_lease_expired')
                    if kind == 'unacknowledged':
                        return 200, dict(success=False)
                if path == '/api/client/heartbeat' and body.get('status') == 'running' and kind == 'heartbeat':
                    return 503, dict(error='temporary_unavailable')
                if path == '/api/range/complete':
                    if kind == 'completion':
                        return 503, dict(error='temporary_unavailable')
                    if kind == 'lease-completion':
                        return 409, dict(error='invalid_assignment_state')
                    if kind == 'permanent':
                        return 403, dict(error='forbidden')
                return super().handle(path, body)

            def respond(instance, handler, code, response):
                if (kind == 'lost-claim' and handler.path == '/api/range/claim'
                    or kind == 'lost-completion' and handler.path == '/api/range/complete'):
                    handler.connection.shutdown(socket.SHUT_RDWR)
                    handler.connection.close()
                    handler.close_connection = True
                    return
                return super().respond(handler, code, response)
        with patch.object(c, 'LocalAssignmentServer', Fault):
            yield

    def inspected(self):
        return c.inspect(self.store, self.directory, CLI, allow_complete=True)[0]

    def calls(self, suffix):
        return [row for row in e.read_json(self.directory/'http-requests.json') if row['path'].endswith(suffix)]

    def engine_logs(self):
        return {str(p.relative_to(self.directory)):s.sha(p) for p in (self.directory/'native').glob('segment-*/attempt-*/engine.log')}

    def test_network_probe_resumes_same_assignment_before_first_kernel(self):
        with self.fault('network-before'):
            self.assertEqual(self.start(), 2)
        seal = self.inspected()
        self.assertEqual((seal['native']['phase'],seal['native']['launches'],seal['recovery']['kind']),('paused',0,'network'))
        self.assertFalse((self.directory/c.ACTIVE).exists())
        self.assertEqual(self.resume(),0)
        self.assertEqual((len(self.calls('/claim')),self.inspected()['native']['completions']),(1,1))

    def test_progress_outage_reconciles_only_durable_batches(self):
        with self.fault('network-during'):
            self.assertEqual(self.start(),3)
        before = self.inspected()
        self.assertTrue(0 < before['native']['checked'] <= c.SEGMENT)
        self.assertLess(before['recovery']['acknowledged'],before['native']['checked'])
        logs = self.engine_logs()
        self.assertEqual(self.resume(),0)
        self.assertEqual(self.inspected()['native']['checked'],c.TOTAL)
        for name,digest in logs.items():
            self.assertEqual(s.sha(self.directory/name),digest)
        self.assertEqual((len(self.calls('/claim')),len(self.calls('/complete'))),(1,1))

    def test_failed_running_heartbeat_recovers_before_launch(self):
        with self.fault('heartbeat'):
            self.assertEqual(self.start(),2)
        self.assertEqual(self.inspected()['recovery']['operation'],'heartbeat')
        self.assertEqual(self.inspected()['native']['launches'],0)
        self.assertEqual(self.resume(),0)
        self.assertEqual(len(self.calls('/claim')),1)

    def test_completion_failure_retries_without_any_kernel_or_new_claim(self):
        with self.fault('completion'):
            self.assertEqual(self.start(),1)
        before = self.inspected()
        self.assertEqual((before['native']['phase'],before['native']['checked']),('pending-complete',c.TOTAL))
        self.assertEqual(len(self.calls('/complete')),3)
        logs = self.engine_logs()
        self.assertEqual(self.resume(),0)
        after = self.inspected()
        self.assertEqual((after['native']['launches'],after['native']['completions']),(2,1))
        self.assertEqual(logs,self.engine_logs())
        self.assertEqual(len(self.calls('/claim')),1)
        replies=e.read_json(self.directory/'http-responses.json')
        self.assertEqual(sum(r['applied']['complete'] for r in replies),1)

    def rejected(self,kind,code):
        with self.fault(kind):
            self.assertEqual(self.start(),code)
        seal=self.inspected()
        self.assertEqual((seal['native']['phase'],seal['recovery']['kind']),('rejected','lease'))
        before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'Lease/assignment rejected'):
            self.resume()
        self.assertEqual(before,self.snapshot())
        self.assertEqual((len(self.calls('/claim')),seal['native']['completions']),(1,0))

    def test_expired_lease_blocks_before_kernel(self):
        self.rejected('lease-before',2)
        self.assertEqual(self.inspected()['native']['launches'],0)

    def test_expired_lease_during_kernel_stops_and_cannot_resume(self):
        self.rejected('lease-during',2)
        self.assertEqual(self.inspected()['native']['launches'],1)

    def test_completion_rejection_stays_terminal_without_relaunch(self):
        self.rejected('lease-completion',1)
        self.assertEqual(self.inspected()['native']['checked'],c.TOTAL)
        self.assertEqual(len(self.calls('/complete')),1)

    def ambiguous(self,kind):
        with self.fault(kind):
            with self.assertRaises(RuntimeError):
                self.start()
        self.assertTrue((self.directory/c.ACTIVE).exists())
        before=self.snapshot()
        with self.hardware(),self.assertRaisesRegex(RuntimeError,'Coordinator transaction requires review'):
            c.execute(self.store,self.directory,CLI,'0'*64)
        self.assertEqual(before,self.snapshot())
        self.assertEqual(len(self.calls('/claim')),1)

    def test_lost_claim_reply_never_claims_again(self):
        self.ambiguous('lost-claim')
        self.assertEqual(self.engine_logs(),{})

    def test_lost_applied_completion_reply_is_not_replayed(self):
        self.ambiguous('lost-completion')
        self.assertEqual(len(self.calls('/complete')),1)
        self.assertEqual(sum(r['applied']['complete'] for r in e.read_json(self.directory/'http-responses.json')),1)

    def test_permanent_error_and_false_ack_stay_for_review(self):
        self.ambiguous('unacknowledged')
        self.assertEqual(self.engine_logs(),{})

    def test_permanent_completion_error_is_not_retried(self):
        self.ambiguous('permanent')
        self.assertEqual(len(self.calls('/complete')),1)

    def test_changed_response_journal_refused_before_resume(self):
        with self.fault('network-before'):
            self.assertEqual(self.start(),2)
        path=self.directory/'http-responses.json'
        data=e.read_json(path);data[-1]['status']=200;s.write_json(path,data)
        before=self.snapshot()
        with self.assertRaisesRegex(RuntimeError,'Coordinator evidence changed'):
            self.resume()
        self.assertEqual(before,self.snapshot())

    def test_stale_native_outcome_cannot_seal_a_new_invocation(self):
        with self.fault('network-before'):
            self.assertEqual(self.start(),2)
        seal=self.inspected();old_hash=s.sha(self.directory/c.SEAL)
        class FailedInvocation:
            returncode=2
            def poll(instance):return 2
        with self.hardware(),patch.object(c.subprocess,'Popen',return_value=FailedInvocation()):
            with self.assertRaisesRegex(RuntimeError,'Stale native invocation outcome'):
                c.execute(self.store,self.directory,CLI,old_hash)
        self.assertTrue((self.directory/c.ACTIVE).exists())
        self.assertEqual(s.sha(self.directory/c.SEAL),old_hash)

    def test_network_recovery_uses_public_run_session_resume(self):
        with self.fault('network-before'):
            self.assertEqual(self.start(),2)
        tools=self.root/'tools';tools.mkdir();gpu=self.record['uuid']
        inventory=tools/'nvidia-smi'
        inventory.write_text('#!/usr/bin/env python3\nimport sys\nquery=sys.argv[1]\n'+
            'if query.startswith("--query-compute-apps"):pass\n'+
            'elif query=="--query-gpu=uuid,name,driver_version":print("'+gpu+', NVIDIA GeForce RTX 4070 SUPER, synthetic-driver")\n'+
            'elif query.startswith("--query-gpu=uuid,temperature.gpu"):print("'+gpu+',30,20,245,0,1000,1000,50")\n'+
            'else:raise SystemExit("Unexpected GPU write/query")\n')
        inventory.chmod(0o700)
        env=dict(os.environ,PATH=str(tools)+os.pathsep+os.environ['PATH'],PYTHONDONTWRITEBYTECODE='1')
        prefix=[str(CLI),'run','20','--engine','vanity','--backend','cuda','--vanity-session']
        listed=subprocess.run(prefix+['runs','--store',str(self.store)],env=env,capture_output=True,text=True,timeout=15)
        self.assertEqual(listed.returncode,0,listed.stderr)
        row=json.loads(listed.stdout)['runs'][0]
        self.assertTrue(row['available']);self.assertEqual(row['recovery'],'network')
        result=subprocess.run(prefix+['resume','--store',str(self.store),'--run-root',str(self.directory),
            '--expected-state',row['state_sha256'],'--enable-experimental-vanity-local'],env=env,capture_output=True,text=True,timeout=20)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertEqual((len(self.calls('/claim')),self.inspected()['native']['completions']),(1,1))


if __name__=='__main__':
    unittest.main(verbosity=2)
