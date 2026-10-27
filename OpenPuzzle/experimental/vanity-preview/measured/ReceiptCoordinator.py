"""GPL-3.0-only. Owned loopback HTTP adapter for the isolated receipt reference.

No deployed API, credentials, desktop store migration or production fallback.
Fault injection belongs exclusively to tests. Ledger effects/receipts remain
one transaction; receipt queries use the reference's read-only connection.
"""
from contextlib import closing
from LocalCoordinator import LocalAssignmentServer, START, require
from ServerContract import Ledger, PROTOCOL


class ReceiptAssignmentServer(LocalAssignmentServer):
    def __init__(self, root, *, ledger_path, **options):
        super().__init__(root, **options)
        try:
            scope=dict(puzzle=20,range_id=37,start=f'{START:064x}',end=f'{START+self.count-1:064x}',
                       target=self.target,required_backend='cuda')
            self.ledger=Ledger(ledger_path,total=self.count,scope=scope)
        except BaseException:
            self.close()
            raise

    def handle(self, path, body):
        require(not self._closed,'Closed receipt peer refuses requests')
        if path in ('/api/client/register','/api/client/heartbeat'):
            return super().handle(path,body)
        require(path in ('/api/vanity/v1/operations','/api/vanity/v1/receipts/query'),
                'Only explicit receipt routes are supported')
        require(body.get('client_id')==self.client_id,'Receipt client differs')
        self.seen[path]=self.seen.get(path,0)+1
        if path.endswith('/query'):return 200,self.ledger.query(body)
        return 200,self.ledger.execute(body)

    def persist(self,pending):
        # A timed-out accepted connection must not publish a new journal/head
        # after the owner closes the peer and seals its evidence.
        require(not self._closed,'Closed receipt peer refuses journal publication')
        return super().persist(pending)

    def options(self):
        return dict(super().options(), **{'--receipt-protocol':PROTOCOL})

    def stats(self):
        """Read-only applied operation counts; queried/replayed receipts add no effect."""
        import json
        result=dict(claim=0,progress=0,complete=0)
        with closing(self.ledger.connect(readonly=True)) as db:
            for row in db.execute('SELECT receipt FROM receipts'):
                r=json.loads(row[0])
                if r['outcome']=='applied':result[r['kind']]+=1
        return result
