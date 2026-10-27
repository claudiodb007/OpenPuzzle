"""GPL-3.0-only. Evidence gates for explicit local network/lease recovery.

No acquisition, HTTP call, checkpoint rewrite or automatic resume happens here.
Native outcomes and the owned local server journal must agree before sealing.
"""
import hashlib
import Evidence as e
import Resume as r
import Support as s


def http_state(directory, meta, checked, phase, data=None):
    if data is None:
        calls = e.read_json(r.private_file(directory, 'http-requests.json'))
        replies = e.read_json(r.private_file(directory, 'http-responses.json'))
    else:
        calls,replies=data
    e.require(len(calls) == len(replies), 'Incomplete local HTTP response journal')
    registrations = claims = completions = 0
    attempted = acknowledged = 0
    for call, event in zip(calls, replies):
        e.require(set(call) == {'path', 'payload'} and set(event) == {'path', 'payload', 'status', 'reply', 'applied'}
                  and call['path'] == event['path'] and call['payload'] == event['payload'], 'HTTP request/response identity differs')
        path, body = call['path'], call['payload']
        applied = event['applied']
        e.require(set(applied) == {'claim', 'progress', 'complete', 'solution'}
                  and all(type(v) is bool for v in applied.values()) and not applied['solution'], 'Unreviewed HTTP effects')
        e.require(body.get('client_id') == meta['client_id'], 'HTTP client differs')
        success = event['status'] == 200 and event['reply'] == {'success': True}
        e.require(type(event['status']) is int and 100 <= event['status'] <= 599, 'Invalid HTTP status evidence')
        if path == '/api/client/register':
            registrations += 1
            e.require(success and body == dict(client_id=meta['client_id']) and not any(applied.values()), 'Unconfirmed local registration')
        elif path == '/api/client/heartbeat':
            e.require(body.get('engines') == [dict(name='Vanity', backend='cuda', installed=True, available=True)]
                      and not any(applied.values()), 'Heartbeat metadata differs')
        elif path == '/api/range/claim':
            claims += 1
            expected = dict(assignment_id='coordinator-assignment-A', puzzle=20, range_id=37, target=meta['target'],
                search_mode='linear', required_backend='cuda', start=f'{s.BASE:064x}', end=f'{s.BASE+2**36-1:064x}')
            e.require(event['status'] == 200 and event['reply'] == expected
                      and applied == dict(claim=True, progress=False, complete=False, solution=False), 'Claim outcome is ambiguous')
            e.require(body.get('puzzle') == 20 and body.get('backend') == 'cuda' and body.get('target_duration_minutes') == 60
                      and abs(float(body.get('speed_mkeys', -1))-meta['planning_mkeys']) < .000001, 'Claim metadata differs')
        elif path in ('/api/range/progress', '/api/range/complete'):
            e.require(body.get('assignment_id') == 'coordinator-assignment-A'
                      and not applied['claim'] and not applied['solution'], 'HTTP assignment differs')
            raw = body.get('keys_checked', '')
            e.require(isinstance(raw, str) and raw.isascii() and raw.isdigit(), 'Invalid HTTP count')
            n = int(raw)
            if path.endswith('/progress'):
                e.require(not completions and attempted <= n <= checked and not applied['complete'], 'HTTP progress regressed/escaped')
                attempted = n
                e.require(success == applied['progress'], 'Unconfirmed progress effect')
                if applied['progress']:
                    e.require(acknowledged <= n, 'Acknowledged progress regressed')
                    acknowledged = n
            else:
                e.require(n == 2**36 and body.get('status') == 'completed' and body.get('exit_code') == 0
                          and acknowledged == n and not applied['progress'], 'Invalid completion evidence')
                e.require(success == applied['complete'], 'Completion outcome is ambiguous')
                completions += int(applied['complete'])
                e.require(completions <= 1, 'Completion applied more than once')
        else:
            raise RuntimeError('Unexpected local endpoint')
    e.require(claims == registrations == 1 and completions == (1 if phase == 'done' else 0)
              and acknowledged <= checked, 'Single assignment outcome differs or is ambiguous')
    return dict(acknowledged=acknowledged, completions=completions)


def outcome(directory, meta, native, exit_code=None, owner_token=None, http_data=None):
    raw = r.private_file(directory, 'native/session-binding.txt').read_bytes()
    state = r.private_file(directory, 'native/coordinator-state.txt').read_text().splitlines()
    rows = r.private_file(directory, 'native/coordinator-outcome.txt').read_text().splitlines()
    e.require(len(rows) == 3 and rows[:2] == ['OPV-RECOVERY-1', hashlib.sha256(raw).hexdigest()], 'Native recovery binding differs')
    fields = rows[2].split()
    e.require(len(fields) == 10, 'Invalid native recovery outcome')
    kind, operation, status, code, phase, offset, checked, launches, pid, token = fields
    e.require(' '.join((phase, offset, checked, launches)) == state[2]
              and phase == native['phase'] and int(checked) == native['checked']
              and int(launches) == native['launches'], 'Stale native recovery outcome')
    status, code = int(status), int(code)
    e.require(int(pid)>1 and token, 'Missing native invocation identity')
    if owner_token is not None:
        e.require(token==owner_token, 'Stale native invocation outcome')
    if exit_code is not None:
        e.require(code == exit_code, 'Native exit and durable outcome differ')
    journal = http_state(directory, meta, native['checked'], phase, http_data)
    transient = status in (0, 408, 429) or 500 <= status <= 599
    valid = (phase == 'done' and kind == 'done' and operation == 'complete' and code == 0 and status == 200
             or phase == 'paused' and kind == 'paused' and operation == 'control' and code == 130
             or phase == 'paused' and kind == 'network' and operation in ('progress', 'heartbeat') and code in (2, 3) and transient
             or phase == 'pending-complete' and kind == 'network' and operation == 'complete' and code == 1
                and status in (408, 429, *range(500, 600))
             or phase == 'rejected' and kind == 'lease' and operation in ('progress', 'heartbeat', 'complete') and code in (1, 2) and 400 <= status <= 599)
    e.require(valid, 'Native outcome requires review; recovery is not authorized')
    e.require(kind not in ('done', 'paused') or journal['acknowledged'] == native['checked'], 'Normal outcome lacks acknowledged progress')
    return dict(kind=kind, operation=operation, http_status=status, exit_code=code, acknowledged=journal['acknowledged'], owner_pid=int(pid), owner_token=token)
