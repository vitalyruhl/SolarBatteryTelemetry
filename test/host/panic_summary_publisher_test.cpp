// [MOCKED!] Synthetic clock, wire and historical summary; no broker or device.
#include "diagnostics/PanicSummaryPublisher.h"
#include "smoke/IsolatedTestProfile.h"
#include <cassert>
#include <string>
#include <cstdio>
struct Wire:telemetry::Transport {
 bool online=false,accept=false;unsigned calls=0;std::string topic,payload;
 bool connected()const override{return online;}
 bool publish(const char* t,const char* p,bool retain)override{assert(!retain);++calls;topic=t;payload=p;return accept;}
 void disconnect()override{}void configure(const char*)override{}
};
int main(){
 using namespace sbt::diagnostics;
 PanicSummary s;s.imageError=261;PanicSummaryPublisher p;Wire wire;
 assert(p.capture(s));s.imageError=999;assert(!p.capture(s));
 p.tick(0,"SBTTest/COM8",wire);assert(wire.calls==0);
 wire.online=true;p.tick(0xffffff00u,"SBTTest/COM8",wire);assert(wire.calls==1);
 assert(wire.topic=="SBTTest/COM8/Diagnostics/Panic");
 assert(sbt::testprofile::topicAllowed(wire.topic.c_str()));
 assert(!sbt::testprofile::topicAllowed("SolarBatteryTelemetry/Diagnostics/Panic"));
 assert(wire.payload.find("\"image_error\":261")!=std::string::npos);
 p.tick(uint32_t(0xffffff00u+4999u),"SBTTest/COM8",wire);assert(wire.calls==1);
 wire.accept=true;p.tick(uint32_t(0xffffff00u+5000u),"SBTTest/COM8",wire);assert(wire.calls==2);
 wire.online=false;p.tick(10000,"SBTTest/COM8",wire);wire.online=true;
 p.tick(20000,"SBTTest/COM8",wire);assert(wire.calls==2); // No replay after reconnect.
 PanicSummaryPublisher fail;assert(fail.capture(s));wire.accept=false;wire.calls=0;
 for(unsigned i=0;i<30;++i)fail.tick(i*5000,"SBTTest/COM8",wire);
 assert(wire.calls==12);
 char longestRoot[telemetry::rootLimit+1];std::memset(longestRoot,'x',telemetry::rootLimit);longestRoot[telemetry::rootLimit]=0;
 PanicSummaryPublisher maximum;maximum.capture(s);wire.calls=0;maximum.tick(0,longestRoot,wire);assert(wire.calls==1);
 assert(wire.topic.size()+wire.payload.size()+7<=mqttBufferBytes);
 std::puts("[MOCKED!] Panic publication isolation/immutable capture/12 attempts/5s wrap/no replay/wire bounds passed.");
}
