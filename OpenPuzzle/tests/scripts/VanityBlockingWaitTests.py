#!/usr/bin/env python3
"""Exercise the actual native CUDA scheduling helper with controlled API failures."""
from pathlib import Path
import subprocess
import sys
import tempfile

source = Path(sys.argv[1]).read_text()
start = source.index('static void configureBlockingSynchronization() {')
end = source.index('\nint main(', start)
helper = source[start:end]
assert source.count('    configureBlockingSynchronization();') == 1
assert source.index('Exactly one visible CUDA GPU required') < source.index('    configureBlockingSynchronization();')
assert source.index('    configureBlockingSynchronization();') < source.index('GPUEngine gpu(')
program = '''#include <iostream>
#include <stdexcept>
constexpr int cudaSuccess=0;
constexpr unsigned cudaDeviceScheduleMask=7,cudaDeviceScheduleBlockingSync=4;
int scenario=0,reads=0,sets=0;unsigned original=0,requested=0;
int cudaGetDeviceFlags(unsigned* flags){++reads;if((scenario==1&&reads==1)||(scenario==3&&reads==2))return 1;*flags=reads==1?original:(scenario==4?1:requested);return 0;}
int cudaSetDeviceFlags(unsigned flags){++sets;requested=flags;return scenario==2?1:0;}
''' + helper + '''
int main(){for(unsigned initial: {0u,1u,2u,4u,8u,16u,128u,139u})for(int mode=0;mode<5;++mode){
original=initial;scenario=mode;reads=sets=0;bool ok=false;try{configureBlockingSynchronization();ok=true;}catch(const std::runtime_error&){}
if(ok!=(mode==0))return 1;
if(mode==0&&requested!=((initial&~7u)|4u))return 2;
if(mode==1&&sets)return 3;
}return 0;}
'''
with tempfile.TemporaryDirectory(prefix='vanity-blocking-wait-') as temporary:
    cpp = Path(temporary)/'probe.cpp'; binary = Path(temporary)/'probe'
    cpp.write_text(program)
    subprocess.run([sys.argv[2], '-std=c++17', str(cpp), '-o', str(binary)], check=True, timeout=60)
    subprocess.run([str(binary)], check=True, stdout=subprocess.DEVNULL, timeout=10)
print('VanityBlockingWaitTests passed: existing context flags preserved; API errors and wrong readback rejected. CPU stubs only.')
