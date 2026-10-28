// [MOCKED!] SDK capture scenarios run as separate processes to preserve the boot-once contract.
#include "diagnostics/PanicSummaryCapture.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include "panic_stubs/AbortElfFixture.h"
static int scenario=0,images=0,checks=0,summaries=0;static Reader fixture;
extern "C" void panic_abort(const char*){std::abort();}
int esp_ota_get_app_elf_sha256(char* out,std::size_t size){assert(size==17);std::memcpy(out,"aaaaaaaaaaaaaaaa",17);return 16;}
int esp_partition_read(const esp_partition_t*,std::size_t at,void* out,std::size_t n){return fixture.read(static_cast<uint32_t>(at),out,n)?0:-1;}
int esp_core_dump_image_get(std::size_t* a,std::size_t* n){++images;*a=0x3f0000;*n=scenario==2?0x10001:scenario>=6?512:12345;return scenario==1?261:0;}
int esp_core_dump_image_check(){++checks;return scenario==3?265:0;}
int esp_core_dump_get_summary(esp_core_dump_summary_t* s){++summaries;s->exc_pc=0x400d1234;s->exc_bt_info.depth=16;s->exc_bt_info.bt[15]=UINT32_MAX;s->ex_info.exc_cause=28;std::memset(s->app_elf_sha256,'a',16);if(scenario>=6){s->exc_pc=scenario==8?0x40083856:0x40083855;s->ex_info.exc_a[2]=0x3ffb1000;std::memcpy(s->app_elf_sha256,scenario==7?"ffffffffffffffff":scenario==10?"fcd49c79dac3bfb7":"35d6636347191fec",17);}
 if(scenario==11)s->app_elf_sha256[16]='x';return scenario==4?-1:0;}
const esp_partition_t* esp_partition_find_first(int,int,const char*){static const esp_partition_t p{0x3f0000,0x10000};return scenario==5?nullptr:&p;}
int main(int argc,char** argv){
 assert(argc==2);scenario=std::atoi(argv[1]);fixture.fail=scenario==9;
 const auto& s=sbt::diagnostics::captureStoredPanicOnce();
 const auto& again=sbt::diagnostics::captureStoredPanicOnce();assert(&s==&again&&images==1);
 assert(checks==((scenario==1||scenario==2||scenario==5)?0:1));
 assert(summaries==((scenario==0||scenario==4||scenario>=6)?1:0));
 if(scenario==0){assert(s.pc==0x400d1234&&s.cause==28&&s.backtrace[15]==UINT32_MAX);assert(s.elfSha256Chars==16&&s.elfSha256[16]==0);}
 char payload[telemetry::payloadLimit+1];assert(sbt::diagnostics::formatPanicSummary(s,payload,sizeof(payload)));
 if(scenario==1||scenario==2||scenario==3||scenario==5)assert(std::strstr(payload,"\"dump_valid\":false"));
 if(scenario==0){assert(std::strstr(payload,"\"elf_sha256_hex_chars\":16"));assert(std::strstr(payload,"\"elf_sha256_complete\":false"));}
 if(scenario==4)assert(std::strstr(payload,"\"summary_error\":-1")&&!std::strstr(payload,"\"pc\""));
 if(scenario==6||scenario==10){assert(s.abortDetails.valid&&s.abortDetails.callerPc==0x4011b064&&s.abortDetails.core==0);assert(std::strstr(payload,"\"abort_caller_pc\":\"0x4011b064\""));}
 else {assert(!s.abortDetails.valid);assert(std::strstr(payload,"\"abort_caller_pc\":null"));}
 if(scenario==6||scenario==9||scenario==10)assert(fixture.reads>0);else assert(fixture.reads==0);
 std::puts("[MOCKED!] SDK capture once / partition bounds / CRC / decoder status passed.");
}
