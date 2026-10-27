// [MOCKED!] Synthetic RTC retention and clock; no abort or hardware access.
#include "smoke/PanicOncePolicy.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace sbt::testprofile;
 volatile PanicOnceLatch rtc{};PanicOncePolicy p;
 assert(!p.ready(10,false,rtc));assert(!p.ready(100,true,rtc));
 assert(!p.ready(60099,true,rtc));assert(p.ready(60100,true,rtc));
 assert(rtc.magic!=0&&rtc.inverse==~rtc.magic);
 assert(!p.ready(120100,true,rtc));PanicOncePolicy reboot;
 assert(!reboot.ready(0,true,rtc));assert(!reboot.ready(60000,true,rtc));
 rtc.magic=0;rtc.inverse=0;PanicOncePolicy interrupted;
 assert(!interrupted.ready(0,true,rtc));assert(!interrupted.ready(59999,false,rtc));
 assert(!interrupted.ready(60000,true,rtc));assert(!interrupted.ready(119999,true,rtc));
 assert(interrupted.ready(120000,true,rtc));
 rtc.magic=0;rtc.inverse=0;PanicOncePolicy wrap;assert(!wrap.ready(0xffffff00u,true,rtc));
 assert(!wrap.ready(uint32_t(0xffffff00u+59999u),true,rtc));
 assert(wrap.ready(uint32_t(0xffffff00u+60000u),true,rtc));
 rtc.magic=0x53425031u;rtc.inverse=~0x53425031u;PanicOncePolicy newGeneration;
 assert(!newGeneration.ready(100,true,rtc));
 assert(newGeneration.ready(60100,true,rtc));
 assert(rtc.magic==0x53425032u&&rtc.inverse==~0x53425032u);
 PanicOncePolicy rebootNewGeneration;assert(!rebootNewGeneration.ready(0,true,rtc));assert(!rebootNewGeneration.ready(60000,true,rtc));
 std::puts("[MOCKED!] Panic once latch / 60s healthy interval / health reset / wrap guards passed.");
}
