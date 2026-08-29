#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path
import sys

P = 2**256 - 2**32 - 977
ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"

EXPECTED = {
    140: (
        "1QKBaU6WAeycb3DbKbLBkX7vJiaS8r42Xo",
        "ffbb35a7bb9bbe16c1aa2534f7ff11d59c8e3d1a",
        "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640",
    ),
    145: (
        "19GpszRNUej5yYqxXoLnbZWKew3KdVLkXg",
        "5abf369388deb8072741b4eb43ef10fa9388a729",
        "03afdda497369e219a2c1c369954a930e4d3740968e5e4352475bcffce3140dae5",
    ),
    150: (
        "1MUJSJYtGPVGkBCTqGspnxyHahpt5Te8jy",
        "e08c4d3bc9cf2b3e2cb88de2bfaa4fe8c7aa3f24",
        "03137807790ea7dc6e97901c2bc87411f45ed74a5629315c4e4b03a0a102250c49",
    ),
    155: (
        "1AoeP37TmHdFh8uN72fu9AqgtLrUwcv2wJ",
        "6b8b7830f73c5bf9e8beb9f161ad82b3bde992e4",
        "035cd1854cae45391ca4ec428cc7e6c7d9984424b954209a8eea197b9e364c05f6",
    ),
    160: (
        "1NBC8uXJy1GiJ6drkiZa1WuKn51ps7EPTv",
        "e84818e1bf7f699aa6e28ef9edfb582099099292",
        "02e0a8b039282faf6fe0fd769cfbc4b6b4cf8758ba68220eac420e32b91ddfa673",
    ),
}


def sha256(value):
    return hashlib.sha256(value).digest()


def hash160(value):
    return hashlib.new("ripemd160", sha256(value)).digest()


def base58check(payload):
    raw = payload + sha256(sha256(payload))[:4]
    number = int.from_bytes(raw, "big")
    encoded = ""
    while number:
        number, remainder = divmod(number, 58)
        encoded = ALPHABET[remainder] + encoded
    leading_zeroes = len(raw) - len(raw.lstrip(b"\0"))
    return "1" * leading_zeroes + encoded


def validate_compressed_point(public_key):
    raw = bytes.fromhex(public_key)
    assert len(raw) == 33 and raw[0] in (2, 3)
    x = int.from_bytes(raw[1:], "big")
    assert x < P
    y_squared = (pow(x, 3, P) + 7) % P
    y = pow(y_squared, (P + 1) // 4, P)
    assert pow(y, 2, P) == y_squared
    if y % 2 != raw[0] % 2:
        y = P - y
    assert y % 2 == raw[0] % 2


def main():
    puzzle_dir = Path(sys.argv[1])

    linear = json.loads((puzzle_dir / "71.json").read_text())
    assert linear["search_mode"] == "linear"
    assert linear["address"] == "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU"
    assert linear["keyspace"] == "400000000000000000:7FFFFFFFFFFFFFFFFF"

    for number, (address, expected_hash160, public_key) in EXPECTED.items():
        item = json.loads((puzzle_dir / f"{number}.json").read_text())
        assert item["number"] == number
        assert item["search_mode"] == "kangaroo"
        assert item["required_backend"] == "cuda"
        assert item["sharing"] == "private_external_readonly"
        assert item["address"] == address
        assert item["hash160"] == expected_hash160
        assert item["public_key"] == public_key

        validate_compressed_point(public_key)
        calculated_hash160 = hash160(bytes.fromhex(public_key))
        assert calculated_hash160.hex() == expected_hash160
        assert base58check(b"\x00" + calculated_hash160) == address

        start_text, end_text = item["keyspace"].split(":")
        assert int(start_text, 16) == 1 << (number - 1)
        assert int(end_text, 16) == (1 << number) - 1

    print("Puzzle catalog integrity: 71 linear + 140/145/150/155/160 kangaroo valid")


if __name__ == "__main__":
    main()
