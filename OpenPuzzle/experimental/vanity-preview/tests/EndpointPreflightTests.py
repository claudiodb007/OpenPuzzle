"""Offline endpoint proposal validation; network use is forbidden in this suite."""
import copy
from pathlib import Path
import socket
import sys
import unittest
from unittest.mock import patch
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'measured'))
from ContractPreflight import check, ROUTES
from ServerContract import PROTOCOL


def manifest(url='http://127.0.0.1:9000'):
    return dict(schema='openpuzzle_vanity_endpoint_proposal_v1',protocol=PROTOCOL,base_url=url,
        routes=dict(ROUTES),capabilities=['atomic_effect_receipt','operation_deduplication',
        'readonly_receipt_query','lease_epoch_fencing'],production_enabled=False)


class Tests(unittest.TestCase):
    def test_valid_configuration_still_blocks_production_without_network(self):
        with patch.object(socket,'socket',side_effect=AssertionError('Network forbidden')), \
             patch.object(socket,'getaddrinfo',side_effect=AssertionError('DNS forbidden')):
            for url in ('http://127.0.0.1:9000','http://[::1]:9000','https://example.invalid'):
                m=manifest(url);before=copy.deepcopy(m);r=check(m)
                self.assertTrue(r['configuration_valid']);self.assertEqual(r['readiness'],'blocked')
                self.assertFalse(r['production_enabled']);self.assertFalse(r['network_contacted'])
                self.assertEqual(len(r['blockers']),4);self.assertEqual(m,before)

    def test_production_true_and_boolean_aliases_refused(self):
        for value in (True,0,None,'false'):
            m=manifest();m['production_enabled']=value
            with self.assertRaises(RuntimeError):check(m)

    def test_remote_plaintext_and_malformed_origins_refused(self):
        for url in ('http://example.invalid','ftp://127.0.0.1','https://user:pass@example.invalid',
                    'https://example.invalid/path','https://example.invalid/','https://example.invalid?x=1',
                    'https://example.invalid#x','https://example.invalid:0','https://example.invalid:65536',
                    'https://example.invalid\\@localhost','https://example.invalid\n',
                    'https://%65xample.invalid','//example.invalid','https://'):
            with self.subTest(url=url),self.assertRaises((RuntimeError,ValueError)):check(manifest(url))

    def test_legacy_routes_cannot_claim_receipt_support(self):
        m=manifest();m['routes']['execute']='/api/range/claim'
        with self.assertRaises(RuntimeError):check(m)

    def test_missing_duplicated_and_unknown_capabilities_refused(self):
        for caps in ([],['atomic_effect_receipt']*4,manifest()['capabilities']+['automatic_resume']):
            m=manifest();m['capabilities']=caps
            with self.assertRaises(RuntimeError):check(m)

    def test_secrets_and_extra_fields_refused(self):
        for key in ('token','authorization','enable_production'):
            m=manifest();m[key]='unused'
            with self.assertRaises(RuntimeError):check(m)

    def test_wrong_protocol_or_manifest_version_refused(self):
        for key in ('schema','protocol'):
            m=manifest();m[key]='legacy'
            with self.assertRaises(RuntimeError):check(m)


if __name__=='__main__':unittest.main(verbosity=2)
