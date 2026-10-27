// [MOCKED!] Synthetic SDK summary and MQTT boundary; no device, network or flash.
#include "diagnostics/PanicSummary.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>

int main(){
 using namespace sbt::diagnostics;
 PanicSummary summary;summary.imageError=0x105;
 char payload[telemetry::payloadLimit+1]{};
 assert(formatPanicSummary(summary,payload,sizeof(payload)));
 assert(std::strstr(payload,"\"dump_present\":false"));
 assert(std::strstr(payload,"\"dump_valid\":false"));
 assert(std::strstr(payload,"\"image_error\":261"));
 assert(std::strstr(payload,"\"check_error\":null"));
 assert(std::strstr(payload,"\"summary_error\":null"));
 assert(!std::strstr(payload,"\"task\""));
 summary={};summary.imagePresent=summary.boundsValid=summary.checkAttempted=summary.summaryAttempted=true;
 summary.imageAddress=0x3f0000;summary.imageSize=64000;summary.pc=0x400d1234;summary.cause=28;
 std::memcpy(summary.task,"ble\"\\\n",6);
 std::memset(summary.elfSha256,'a',64);summary.depth=32;
 for(unsigned i=0;i<16;++i)summary.backtrace[i]=0x400d1000+i;
 assert(formatPanicSummary(summary,payload,sizeof(payload)));
 assert(std::strstr(payload,"\"dump_valid\":true"));
 assert(std::strstr(payload,"\"pc\":\"0x400d1234\""));
 assert(std::strstr(payload,"\"cause\":28"));
 assert(std::strstr(payload,"\"task\":\"ble\\\"\\\\\\u000a\""));
 assert(std::strstr(payload,"\"bt_depth\":16"));
 assert(std::strstr(payload,"\"bt_truncated\":true"));
 assert(std::strstr(payload,"\"0x400d100f\""));
 assert(!std::strstr(payload,"0x400d1010"));
 assert(std::strstr(payload,"\"elf_sha256_valid\":true"));
 assert(std::strlen(payload)<=telemetry::payloadLimit);
 const std::string exact=payload;
 char small[16];std::memset(small,'x',sizeof(small));
 assert(!formatPanicSummary(summary,small,sizeof(small))&&small[0]==0);
 assert(!formatPanicSummary(summary,nullptr,0));
 // Maximum escaped task and full backtrace remain inside the existing 1024-byte limit.
 std::memset(summary.task,1,sizeof(summary.task));summary.backtraceCorrupted=true;
 assert(formatPanicSummary(summary,payload,sizeof(payload)));
 assert(std::strlen(payload)<=telemetry::payloadLimit);
 assert(std::strstr(payload,"\"bt_corrupted\":true"));
 summary.pc=UINT32_MAX;summary.cause=UINT32_MAX;summary.depth=UINT32_MAX;
 for(auto& pc:summary.backtrace)pc=UINT32_MAX;
 assert(formatPanicSummary(summary,payload,sizeof(payload)));
 assert(std::strlen(payload)+7+telemetry::topicLimit<=mqttBufferBytes);
 summary.abortDetails.status="ok";summary.abortDetails.valid=true;summary.abortDetails.callerPc=0x403fffff;summary.abortDetails.core=1;
 assert(formatPanicSummary(summary,payload,sizeof(payload)));
 assert(std::strlen(payload)+7+telemetry::topicLimit<=mqttBufferBytes);
 std::printf("[MOCKED!] Maximum task/backtrace/hash + abort-origin payload: %zu bytes\n",std::strlen(payload));
 summary.abortDetails={};summary.elfSha256[64]='a';
 assert(formatPanicSummary(summary,payload,sizeof(payload)));
 assert(std::strstr(payload,"\"elf_sha256_valid\":false"));
 summary.elfSha256[64]=0;summary.elfSha256[2]='!';
 assert(formatPanicSummary(summary,payload,sizeof(payload)));
 assert(std::strstr(payload,"\"elf_sha256_valid\":false"));
 summary.summaryError=-1;
 assert(formatPanicSummary(summary,payload,sizeof(payload)));
 assert(std::strstr(payload,"\"summary_error\":-1")&&!std::strstr(payload,"\"task\""));
 summary.checkError=0x109;summary.summaryAttempted=false;
 assert(formatPanicSummary(summary,payload,sizeof(payload)));
 assert(std::strstr(payload,"\"dump_valid\":false"));
 std::puts("[MOCKED!] Panic summary JSON bounds/status/escaping/backtrace guards passed.");
}
