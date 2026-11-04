import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
import KangarooSparseCheckpointTests as legacy


def patched_source(directory):
    fixture = legacy.ROOT / "tests/fixtures/psckangaroo-checkpoint"
    metadata = json.loads((fixture / "SOURCE-bitmap.json").read_text())
    patch = legacy.ROOT / "scripts/patches/psckangaroo-bitmap-checkpoint-v1.patch"
    assert hashlib.sha256(patch.read_bytes()).hexdigest() == metadata["patch_sha256"]
    target = directory / "TameStore.h"
    target.write_bytes((fixture / "TameStore.h").read_bytes())
    subprocess.run(["git", "apply", "--include=TameStore.h", str(patch)],
                   cwd=directory, check=True, timeout=20)
    assert hashlib.sha256(target.read_bytes()).hexdigest() == metadata["TameStore.h"]
    return target.read_text()


def program(candidate):
    fixture = legacy.program().split("int main(", 1)[0]
    fixture = fixture.replace("bad_write&&size*count>104", "bad_write&&size*count==16")
    begin = candidate.index("#include <algorithm>")
    end = candidate.index("}  // namespace checkpoint_bitmap", begin)
    codec = candidate[begin:end] + "}\n"
    fixture = fixture.replace("#define fsync checked_sync", codec + "#define fsync checked_sync", 1)
    start = fixture.index("struct SparseStore")
    end = fixture.index("#pragma pack(push,1)", start)
    compact = fixture[start:end].replace("SparseStore", "CompactStore")
    begin = candidate.index("    u64 LoadWildsFromCheckpoint(")
    end = candidate.rindex("\n};")
    compact += candidate[begin:end] + "\n};\n"
    return fixture + compact + r'''
#include <cassert>
int main(int argc, char** argv) {
    const std::filesystem::path root = argv[1];
    for (int mode = 0; mode < 6; ++mode) {
        const bool two = mode % 2 != 0;
        DenseStore legacy(two);
        SparseStore sparse(two);
        CompactStore compact(two);
        legacy.fill(mode == 2 || mode == 3, mode >= 4);
        sparse.fill(mode == 2 || mode == 3, mode >= 4);
        compact.fill(mode == 2 || mode == 3, mode >= 4);
        const std::string base = (root / std::to_string(mode)).string();
        const auto old = base + ".old";
        const auto holes = base + ".sparse";
        const auto file = base + ".compact";
        assert(legacy.SaveCheckpoint(old.c_str()));
        assert(sparse.SaveCheckpoint(holes.c_str()));
        assert(compact.SaveCheckpoint(file.c_str()));
        CompactStore reader(two);
        assert(reader.LoadCheckpoint(old.c_str()) && reader.same(legacy));
        assert(reader.LoadCheckpoint(holes.c_str()) && reader.same(sparse));
        assert(reader.LoadCheckpoint(file.c_str()) && reader.same(compact));
        if (two) {
            FILE* legacy_b = fopen(old.c_str(), "r+b");
            assert(legacy_b && fwrite("RCKDT5B", 1, 7, legacy_b) == 7 && fclose(legacy_b) == 0);
            assert(reader.LoadCheckpoint(old.c_str()) && reader.same(legacy));
            CompactStore single(false);
            assert(single.LoadCheckpoint(file.c_str()) && single.same(compact));
            CompactStore preload(true);
            bool error = false;
            assert(preload.LoadWildsFromCheckpoint(file.c_str(), error) == compact.wild_count[1]);
            assert(!error && preload.backing0[0].part1 == 0);
            assert(memcmp(preload.wild_table[1], compact.wild_table[1],
                          compact.wild_table_size * sizeof(WildEntryCompact)) == 0);
        }
    }
    CompactStore writer(true);
    writer.fill(false);
    const auto live = (root / "live").string();
    assert(writer.SaveCheckpoint(live.c_str()));
    for (int fault = 0; fault < 2; ++fault) {
        bad_sync = fault == 0;
        bad_write = fault == 1;
        assert(!writer.SaveCheckpoint(live.c_str()));
        bad_sync = bad_write = false;
        assert(!std::filesystem::exists(live + ".tmp"));
        CompactStore reader(true);
        assert(reader.LoadCheckpoint(live.c_str()) && reader.same(writer));
    }
    for (int fault = 0; fault < 4; ++fault) {
        const auto damaged = live + ".damaged";
        std::filesystem::copy_file(live, damaged, std::filesystem::copy_options::overwrite_existing);
        const auto size = std::filesystem::file_size(damaged);
        if (fault == 0) {
            std::filesystem::resize_file(damaged, size - 1);
        } else {
            FILE* fp = fopen(damaged.c_str(), "r+b");
            const long offset = fault == 1 ? 105 : fault == 2 ? 40 : size - 5;
            assert(fp && fseek(fp, offset, SEEK_SET) == 0);
            const int byte = fgetc(fp);
            assert(byte != EOF && fseek(fp, offset, SEEK_SET) == 0);
            assert(fputc(byte ^ 1, fp) != EOF && fclose(fp) == 0);
        }
        CompactStore reader(true);
        assert(!reader.LoadCheckpoint(damaged.c_str()));
        bool error = false;
        assert(reader.LoadWildsFromCheckpoint(damaged.c_str(), error) == 0 && error);
    }
    puts("PASS: legacy v3, sparse v3, compact v4, preload, table metadata, truncation, CRC, write and fsync failures");
}
'''


with tempfile.TemporaryDirectory() as temp:
    directory = Path(temp)
    source = directory / "probe.cpp"
    source.write_text(program(patched_source(directory)))
    binary = directory / "probe"
    subprocess.run([legacy.CXX, "-std=c++17", "-O2", str(source), "-lz", "-o", str(binary)],
                   check=True, timeout=60)
    result = subprocess.run([str(binary), str(directory)], check=True, capture_output=True,
                            text=True, timeout=30)
    print(result.stdout.splitlines()[-1])
