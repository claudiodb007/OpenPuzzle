"""GPL-3.0-only. Offline configuration check for an undeployed protocol proposal.

No network, credential reads, hardware, native run or production enablement.
Declared capabilities are never treated as evidence of deployed support.
"""
import ipaddress
from urllib.parse import urlsplit
from ServerContract import PROTOCOL, require

ROUTES = {'execute': '/api/vanity/v1/operations', 'query': '/api/vanity/v1/receipts/query'}


def check(manifest):
    require(type(manifest) is dict and set(manifest) == {'schema','protocol','base_url','routes',
            'capabilities','production_enabled'}, 'Explicit endpoint manifest required')
    require(manifest['schema'] == 'openpuzzle_vanity_endpoint_proposal_v1'
            and manifest['protocol'] == PROTOCOL and manifest['production_enabled'] is False,
            'Production must remain disabled')
    raw = manifest['base_url']
    require(isinstance(raw,str) and raw and raw.isascii() and all(ord(c)>32 for c in raw)
            and '\\' not in raw and '%' not in raw, 'Invalid base URL')
    u = urlsplit(raw)
    require(u.scheme in ('http','https') and u.hostname and not u.username and not u.password
            and not u.query and not u.fragment and not u.path and raw == u.scheme+'://'+u.netloc,
            'Canonical origin without credentials/path required')
    require(u.port is None or 1 <= u.port <= 65535, 'Invalid port')
    try:loopback = ipaddress.ip_address(u.hostname).is_loopback
    except ValueError:loopback = u.hostname == 'localhost'
    require(loopback or u.scheme == 'https', 'Remote endpoint requires HTTPS')
    require(type(manifest['routes']) is dict and manifest['routes'] == ROUTES, 'Explicit versioned proposal routes required')
    caps = {'atomic_effect_receipt','operation_deduplication','readonly_receipt_query','lease_epoch_fencing'}
    require(type(manifest['capabilities']) is list and all(isinstance(x,str) for x in manifest['capabilities'])
            and len(manifest['capabilities']) == len(caps) and set(manifest['capabilities']) == caps,
            'Declared contract capabilities differ')
    return dict(schema='openpuzzle_vanity_preflight_v1', configuration_valid=True,
        protocol=PROTOCOL, endpoint=raw, routes=dict(ROUTES), production_enabled=False,
        readiness='blocked', network_contacted=False,
        blockers=['Deployed API implementation and authentication are not verified',
                  'Remote MySQL atomicity and concurrency are not verified',
                  'Native request and receipt recovery integration is not implemented',
                  'Release and production authorization gates remain closed'])
