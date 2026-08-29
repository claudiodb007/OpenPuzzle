#include "openpuzzle/services/PuzzleMetadataCatalog.hpp"

#include <cassert>
#include <string>

using openpuzzle::PuzzleMetadataCatalog;

namespace {

std::string kangarooJson(
    const std::string &publicKey,
    const std::string &backend = "cuda",
    const std::string &address = "1QKBaU6WAeycb3DbKbLBkX7vJiaS8r42Xo",
    const std::string &hash160 = "ffbb35a7bb9bbe16c1aa2534f7ff11d59c8e3d1a",
    const std::string &keyspace =
        "80000000000000000000000000000000000:"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF") {
  return std::string("{\"number\":140,\"search_mode\":\"kangaroo\",") +
         "\"address\":\"" + address + "\"," +
         "\"hash160\":\"" + hash160 + "\"," +
         "\"keyspace\":\"" + keyspace + "\"," +
         "\"public_key\":\"" + publicKey + "\"," +
         "\"required_backend\":\"" + backend + "\"}";
}

} // namespace

int main() {
  {
    const auto metadata = PuzzleMetadataCatalog::parse(
        71,
        R"JSON({
          "number": 71,
          "search_mode": "linear",
          "address": "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU",
          "keyspace": "400000000000000000:7FFFFFFFFFFFFFFFFF"
        })JSON");

    assert(metadata);
    assert(metadata->puzzle == 71);
    assert(metadata->searchMode == "linear");
    assert(metadata->address == "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU");
    assert(metadata->publicKey.empty());
  }

  const std::string publicKey =
      "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640";

  {
    const auto metadata =
        PuzzleMetadataCatalog::parse(140, kangarooJson(publicKey));
    assert(metadata);
    assert(metadata->searchMode == "kangaroo");
    assert(metadata->requiredBackend == "cuda");
    assert(metadata->publicKey == publicKey);
  }

  assert(!PuzzleMetadataCatalog::parse(141, kangarooJson(publicKey)));
  assert(!PuzzleMetadataCatalog::parse(
      140, kangarooJson("synthetic-public-key")));
  assert(!PuzzleMetadataCatalog::parse(
      140, kangarooJson(publicKey, "opencl")));
  assert(!PuzzleMetadataCatalog::parse(
      140, kangarooJson(publicKey, "cuda", "not-a-base58-address")));
  assert(!PuzzleMetadataCatalog::parse(
      140, kangarooJson(publicKey, "cuda",
                        "1QKBaU6WAeycb3DbKbLBkX7vJiaS8r42Xo",
                        "not-a-hash160")));
  assert(!PuzzleMetadataCatalog::parse(
      140, kangarooJson(publicKey, "cuda",
                        "1QKBaU6WAeycb3DbKbLBkX7vJiaS8r42Xo",
                        "ffbb35a7bb9bbe16c1aa2534f7ff11d59c8e3d1a",
                        "FFFFFFFF:80000000")));
  assert(!PuzzleMetadataCatalog::parse(
      140, R"JSON({"number":140,"search_mode":"unknown"})JSON"));

  for (const int puzzle : {140, 145, 150, 155, 160}) {
    const auto metadata = PuzzleMetadataCatalog::load(puzzle);
    assert(metadata);
    assert(metadata->puzzle == puzzle);
    assert(metadata->searchMode == "kangaroo");
    assert(metadata->requiredBackend == "cuda");
  }

  return 0;
}
