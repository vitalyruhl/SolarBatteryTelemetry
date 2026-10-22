#include "telemetry/TelemetryPublisher.h"
#include <cassert>
#include <string>
#include <vector>
#include <cstdio>
using namespace telemetry;
// [MOCKED!] Broker admissions only; snapshot/delta/refresh logic is real.
struct Wire:Transport{std::vector<std::string> sent;std::vector<bool> retain;
 bool connected()const override{return true;}bool publish(const char* t,const char* p,bool r)override{sent.emplace_back(std::string(t)+"="+p);retain.push_back(r);return true;}
 void disconnect()override{}void configure(const char*)override{}
};
static void drain(Publisher& p,Wire& w,unsigned now){for(unsigned i=0;i<20;++i)p.tick(now,w);}
int main(){Publisher p;Wire w;p.setSourceAvailable(Source::Bms,true);p.setValue(channelIndex("bms/charge_enabled"),{true,1});p.onConnected();drain(p,w,0);w.sent.clear();w.retain.clear();
 p.setValue(channelIndex("bms/charge_enabled"),{true,0});drain(p,w,10);
 bool changed=false;for(size_t i=0;i<w.sent.size();++i){if(w.sent[i]=="SolarBatteryTelemetry/bms/charge_enabled=false"){changed=true;assert(w.retain[i]);}assert(w.sent[i].find("/set=")==std::string::npos);}assert(changed);
 w.sent.clear();p.setValue(channelIndex("bms/voltage_v"),{true,51.5});drain(p,w,20);assert(!w.sent.empty());
 w.sent.clear();p.setValue(channelIndex("bms/voltage_v"),{true,51.5});drain(p,w,30);assert(w.sent.empty());
 drain(p,w,60000);assert(!w.sent.empty());
 // Continuous changes must not restart the bounded cursor forever.
 Publisher fair;Wire busy;fair.setSourceAvailable(Source::Bms,true);fair.onConnected();
 for(unsigned tick=0;tick<20;++tick){fair.setValue(channelIndex("bms/charge_enabled"),{true,0});fair.setValue(channelIndex("bms/voltage_v"),{true,50.0+tick});fair.tick(tick,busy);}
 bool reached=false;for(const auto& m:busy.sent)if(m=="SolarBatteryTelemetry/bms/charge_enabled=false")reached=true;assert(reached);
 std::puts("Immediate changed values, retained actual MOS, no status-command echo and full refresh passed");
}
