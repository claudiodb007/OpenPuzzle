import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
CXX=sys.argv.pop(1) if len(sys.argv)>1 else "g++"
sys.dont_write_bytecode=True


def methods(text):
    start=text.index('    bool SaveCheckpoint(const char* filename) {')
    return text[start:text.rindex('\n};')]


def program():
    fixture=ROOT/'tests/fixtures/psckangaroo-checkpoint'
    before=(fixture/'TameStore.h').read_text()
    metadata=json.loads((fixture/'SOURCE.json').read_text())
    assert hashlib.sha256(before.encode()).hexdigest()==metadata['baseline_sha256']
    patch=ROOT/'scripts/patches/psckangaroo-sparse-checkpoint-v1.patch'
    assert hashlib.sha256(patch.read_bytes()).hexdigest()==metadata['patch_sha256']
    with tempfile.TemporaryDirectory() as temporary:
        path=Path(temporary)/'TameStore.h';path.write_text(before)
        for arguments in [('apply','--check',str(patch)),('apply',str(patch)),('apply','--reverse','--check',str(patch))]:
            subprocess.run(['git',*arguments],cwd=temporary,check=True,capture_output=True,timeout=20)
        after=path.read_text()
    assert hashlib.sha256(after.encode()).hexdigest()==metadata['candidate_sha256']
    portability=before[before.index('#ifdef _WIN32\n    #define FSEEK64'):before.index('// ============================================================================\n// CONFIGURATION')]
    types=after[after.index('#pragma pack(push, 1)\nstruct TameEntry'):after.index('\nclass SpinLock')]
    hstart=before.index('    struct CheckpointHeader {');hend=before.index('#pragma pack(pop)',hstart)
    header=before[hstart:hend]
    prefix='''#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>
using u8=uint8_t;using u32=uint32_t;using u64=uint64_t;
bool bad_sync=false,bad_seek=false,bad_write=false;
int checked_sync(int fd){if(bad_sync){errno=EIO;return -1;}return fsync(fd);}
int checked_seek(FILE*fp,off_t offset,int origin){if(bad_seek){errno=EIO;return -1;}return fseeko(fp,offset,origin);}
size_t checked_write(const void*p,size_t size,size_t count,FILE*fp){
 if(bad_write&&size*count>104){errno=ENOSPC;return 0;}return fwrite(p,size,count,fp);}
#define fsync checked_sync
#define fseeko checked_seek
#define fwrite checked_write
#define SPATIAL_BUCKETS 65536
struct SpatialBucket{std::atomic<u64> write_index{0},fill_count{0};};
'''
    fields='''
 bool initialized=true;WildEntryCompact*wild_table[2]{};u64 wild_table_size;
 std::atomic<u64>wild_count[2]{},table_overwrites[2]{};bool circular_mode_active[2]{};
 std::vector<WildEntryCompact> backing0,backing1;std::unique_ptr<SpatialBucket[]> spatial_buckets;
 explicit CLASSNAME(bool two):wild_table_size(131072),backing0(131072),backing1(two?131072:0),spatial_buckets(new SpatialBucket[65536]){
 wild_table[0]=backing0.data();wild_table[1]=two?backing1.data():nullptr;}
 void fill(bool dense,bool empty=false){for(int t=0;t<2;++t){if(!wild_table[t])continue;
 if(!empty)for(u64 i=0;i<wild_table_size;++i)if(dense||i==0||i==17||i==wild_table_size/2||i==wild_table_size-1)
 wild_table[t][i]={i+1+u64(t),0x1234567890abcdefULL^i};
 wild_count[t].store(empty?0:dense?wild_table_size:4);table_overwrites[t].store(17+t);circular_mode_active[t]=(t==1);}
 for(u32 i=0;i<SPATIAL_BUCKETS;++i){spatial_buckets[i].write_index.store(i+7);spatial_buckets[i].fill_count.store(i%13);}}
 template<class Other>bool same(Other&other){
 for(int t=0;t<2;++t){if(wild_table[t]&&memcmp(wild_table[t],other.wild_table[t],wild_table_size*sizeof(WildEntryCompact)))return false;
 if(wild_count[t].load()!=other.wild_count[t].load()||table_overwrites[t].load()!=other.table_overwrites[t].load()||circular_mode_active[t]!=other.circular_mode_active[t])return false;}
 for(u32 i=0;i<SPATIAL_BUCKETS;++i)if(spatial_buckets[i].write_index.load()!=other.spatial_buckets[i].write_index.load()||spatial_buckets[i].fill_count.load()!=other.spatial_buckets[i].fill_count.load())return false;return true;}
'''
    classes=''
    for name,text in [('DenseStore',before),('SparseStore',after)]:
        classes+='struct '+name+'{\n'+fields.replace('CLASSNAME',name)+'\n#pragma pack(push,1)\n'+header+'\n#pragma pack(pop)\n'+methods(text)+'\n};\n'
    main='''
int main(int argc,char**argv){
 static_assert(sizeof(DenseStore::CheckpointHeader)==104);static_assert(sizeof(WildEntryCompact)==16);
 std::filesystem::path root=argv[1];
 for(int mode=0;mode<6;++mode){bool two=mode%2,dense=mode==2||mode==3,empty=mode>=4;
 DenseStore original(two);SparseStore candidate(two);original.fill(dense,empty);candidate.fill(dense,empty);
 std::string prefix=(root/std::to_string(mode)).string();
 if(!original.SaveCheckpoint((prefix+".dense").c_str())||!candidate.SaveCheckpoint((prefix+".sparse").c_str()))return 1;
 DenseStore old_reader(two);SparseStore new_reader(two);
 if(!old_reader.LoadCheckpoint((prefix+".sparse").c_str())||!new_reader.LoadCheckpoint((prefix+".dense").c_str()))return 2;
 if(!old_reader.same(original)||!new_reader.same(original))return 3;
 if(two){DenseStore one_reader(false);if(!one_reader.LoadCheckpoint((prefix+".sparse").c_str())||!one_reader.same(original))return 4;}
 }
 SparseStore protected_store(true);protected_store.fill(false);
 std::string live=(root/"protected.checkpoint").string();if(!protected_store.SaveCheckpoint(live.c_str()))return 5;
 for(int fault=0;fault<3;++fault){bad_sync=fault==0;bad_write=fault==1;bad_seek=fault==2;
 if(protected_store.SaveCheckpoint(live.c_str()))return 6;bad_sync=bad_write=bad_seek=false;
 if(std::filesystem::exists(live+".tmp"))return 7;DenseStore reader(true);
 if(!reader.LoadCheckpoint(live.c_str())||!reader.same(protected_store))return 8;}
 // Exercise the unchanged 64-bit seek API beyond 4 GiB without allocating a huge table.
 std::string wide=(root/"wide-offset.bin").string();FILE*wide_fp=fopen(wide.c_str(),"wb");if(!wide_fp)return 10;
 if(FSEEK64(wide_fp,int64_t(1)<<32,SEEK_SET)!=0)return 11;
 std::vector<WildEntryCompact> zero_entries(131072);
 if(OpenPuzzleWriteCheckpointEntries(zero_entries.data(),zero_entries.size(),wide_fp)!=zero_entries.size())return 12;
 if(fwrite("TAIL",1,4,wide_fp)!=4||FSYNC_FILE(wide_fp)!=0||fclose(wide_fp)!=0)return 13;
 if(std::filesystem::file_size(wide)!=(u64(1)<<32)+zero_entries.size()*16+4)return 14;
 FILE*wide_reader=fopen(wide.c_str(),"rb");char tail[4];
 if(!wide_reader||FSEEK64(wide_reader,-4,SEEK_END)!=0||fread(tail,1,4,wide_reader)!=4||memcmp(tail,"TAIL",4))return 15;
 fclose(wide_reader);
 std::string truncated=(root/"truncated.checkpoint").string();std::filesystem::copy_file(live,truncated);
 std::filesystem::resize_file(truncated,std::filesystem::file_size(truncated)-8);DenseStore bad_reader(true);
 if(bad_reader.LoadCheckpoint(truncated.c_str()))return 9;
 return 0;
}
'''
    return prefix+portability+types+classes+main


class Tests(unittest.TestCase):
    def test_legacy_byte_identity_loader_roundtrip_holes_and_failure_preservation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);cpp=root/'probe.cpp';binary=root/'probe';cpp.write_text(program())
            subprocess.run([CXX,'-std=c++17','-O2',str(cpp),'-o',str(binary)],check=True,timeout=60)
            with (root/'test.log').open('w') as log:
                subprocess.run([str(binary),str(root)],check=True,timeout=20,stdout=log,stderr=subprocess.STDOUT)
            evidence=[]
            for mode in range(6):
                dense=root/(str(mode)+'.dense');sparse=root/(str(mode)+'.sparse')
                self.assertEqual(dense.stat().st_size,sparse.stat().st_size)
                self.assertEqual(hashlib.sha256(dense.read_bytes()).digest(),hashlib.sha256(sparse.read_bytes()).digest())
                row=dict(case=mode,logical_bytes=sparse.stat().st_size,dense_allocated=dense.stat().st_blocks*512,sparse_allocated=sparse.stat().st_blocks*512)
                if mode not in (2,3):self.assertLess(row['sparse_allocated'],row['dense_allocated']*.6)
                evidence.append(row)
            print(json.dumps(dict(status='passed',scope='actual pinned SaveCheckpoint/LoadCheckpoint methods; controlled CPU table fixtures',
                format='RCKDT5C version 3',byte_identical=True,legacy_reader_passed=True,reader_modes_tested=[1,2],
                atomic_preservation_faults=['fsync','write','seek'],truncation_rejected=True,large_seek_offset_checked=True,cases=evidence,
                gpu_executed=False,real_checkpoint_files_changed=False),indent=2)+'\n')


if __name__=='__main__':unittest.main(verbosity=2)
