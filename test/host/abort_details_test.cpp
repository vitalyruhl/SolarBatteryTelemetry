// [MOCKED!] Synthetic ELF32 image and bounded reader; no device or live memory.
#include "diagnostics/AbortDetails.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <array>
#include "panic_stubs/AbortElfFixture.h"

int main(){
 using namespace sbt::diagnostics;
 assert(knownAbortContext("35d6636347191fec",16,0x40083855,"aaaaaaaaaaaaaaaa",0));
 assert(knownAbortContext("fcd49c79dac3bfb7",16,0x40083855,"aaaaaaaaaaaaaaaa",0));
 assert(!knownAbortContext("ffffffffffffffff",16,0x40083855,"aaaaaaaaaaaaaaaa",0));
 assert(!knownAbortContext("35d6636347191fec",16,0x40083856,"aaaaaaaaaaaaaaaa",0));
 assert(knownAbortContext("aaaaaaaaaaaaaaaa",16,0x40012345,"aaaaaaaaaaaaaaaa",0x40012345));
 assert(!knownAbortContext("aaaaaaaaaaaaaaaa",16,0x40012346,"aaaaaaaaaaaaaaaa",0x40012345));
 Reader valid;auto out=resolveAbortDetails(valid,512,0x3ffb1000,true);assert(out.valid&&out.callerPc==0x4011b064&&out.core==0);
 Reader unknown;assert(!resolveAbortDetails(unknown,512,0x3ffb1000,false).valid&&unknown.reads==0);
 Reader bad;bad.bytes[20]=0;assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.bytes[25]=2;assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u32(4,0x101);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u32(0,511);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u32(48,0xfffffff0);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u16(64,161);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u16(62,31);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u32(76,0xfffffff0);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u32(80,0xfffffff0);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u32(92,50);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u32(76,52);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u16(64,2);bad.ph(104,1,256,0x3ffb1000,100,100);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u16(64,2);bad.ph(104,1,256,0x3ffb1004,100,100);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};std::memset(bad.bytes.data()+148,'x',100);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.text("abort() was called at PC 0x4011b064 on core 2");assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.text("abort() was called at PC 0x4011b064 on core 0 extra");assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.text("assert failed secret text");assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.text("abort() was called at PC 0xFFFFFFFF on core 1");assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.text("abort() was called at PC 0x00000000 on core 1");assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};bad.u32(88,20);bad.u32(92,20);assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};assert(!resolveAbortDetails(bad,512,0x3ffb1064,true).valid);
 bad=Reader{};assert(!resolveAbortDetails(bad,512,0x3ffb0fff,true).valid);
 bad=Reader{};bad.fail=true;assert(!resolveAbortDetails(bad,512,0x3ffb1000,true).valid);
 bad=Reader{};assert(!resolveAbortDetails(bad,65537,0x3ffb1000,true).valid);
 bad=Reader{};assert(!resolveAbortDetails(bad,75,0x3ffb1000,true).valid);
 bad=Reader{};bad.text("abort() was called at PC 0x4009b064 on core 1");out=resolveAbortDetails(bad,512,0x3ffb1000,true);assert(out.valid&&out.core==1);
 std::puts("[MOCKED!] Abort-only ELF resolver header/overflow/overlap/bounds/NUL/format/core/context guards passed.");
}
