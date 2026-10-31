// [MOCKED!] Only the MQTT boundary and acquisition clock are synthetic.
#include "telemetry/CellPublisher.h"
#include "telemetry/PacedPublication.h"
#include "telemetry/StatusPublisher.h"
#include "diagnostics/RuntimePublisher.h"
#include "diagnostics/PanicSummaryPublisher.h"
#include "telemetry/VictronHistoryPublisher.h"
#include <cassert>
#include <string>
#include <vector>
#include <cstdio>
struct Wire:telemetry::Transport {
 struct Message {std::string topic,payload;bool retained;};std::vector<Message> sent;
 bool online=true,fail=false;unsigned attempts=0;
 bool connected()const override{return online;}
 bool publish(const char*t,const char*p,bool r)override{++attempts;if(fail)return false;sent.push_back({t,p,r});return true;}
 void disconnect()override{online=false;}void configure(const char*)override{}
};
int main(){
 telemetry::CellPublisher p;Wire w;bms::Snapshot sample;sample.available=sample.cellsValid=true;sample.sequence=7;sample.receivedAt=100;
 for(unsigned i=0;i<14;++i)sample.cellsMv[i]=3300+i;
 p.observe(sample,110,1700000000);p.tick(120,"SBTTest/COM8",w);
 assert(w.sent.size()==1&&!w.sent[0].retained&&w.sent[0].topic=="SBTTest/COM8/bms/cells");
 assert(w.sent[0].payload=="{\"available\":true,\"source\":\"jbd_03_04\",\"sample_sequence\":7,\"sample_age_ms\":20,\"received_at\":1700000000,\"cells_mv\":[3300,3301,3302,3303,3304,3305,3306,3307,3308,3309,3310,3311,3312,3313]}");
 sample.sequence=8;sample.receivedAt=5000;p.observe(sample,5010,1700000005);p.tick(5120,"SBTTest/COM8",w);assert(w.sent.size()==1);
 sample.cellsMv[13]=3314;p.observe(sample,5010,1700000005);w.fail=true;p.tick(5120,"SBTTest/COM8",w);assert(w.attempts==2);
 w.fail=false;p.tick(5369,"SBTTest/COM8",w);assert(w.attempts==2);p.tick(5370,"SBTTest/COM8",w);assert(w.sent.size()==2);
 p.setRefreshInterval(100000);p.tick(60120,"SBTTest/COM8",w);assert(w.sent.size()==2);
 p.tick(100120,"SBTTest/COM8",w);assert(w.sent.size()==3&&w.sent.back().payload.find("\"sample_age_ms\":95120")!=std::string::npos);
 sample.available=sample.cellsValid=false;p.observe(sample,100121,0);p.tick(100121,"SBTTest/COM8",w);
 assert(w.sent.size()==4&&w.sent.back().payload.find("\"cells_mv\":null")!=std::string::npos);
 p.tick(105121,"SBTTest/COM8",w);assert(w.sent.size()==4);
 sample.available=sample.cellsValid=true;sample.sequence=9;sample.receivedAt=0xffffff00u;p.observe(sample,0xffffff10u,0);
 p.tick(0xffffff20u,"SBTTest/COM8",w);assert(w.sent.back().payload.find("\"received_at\":null")!=std::string::npos);
 p.onConnected();p.tick(20,"SBTTest/COM8",w);assert(w.sent.back().payload.find("\"sample_age_ms\":276")!=std::string::npos);
 const auto before=w.sent.size();p.tick(21,"another",w);assert(w.sent.size()==before+1&&w.sent.back().topic=="another/bms/cells");
 // Elapsed admission, failed writes, rollover, round-robin fairness and no catch-up burst.
 telemetry::PacedPublication pace;Wire paced;
 auto visit=[&](unsigned slot,telemetry::Transport& wire){wire.publish(slot==0?"early":"late","1",false);};
 for(unsigned n=0;n<100;++n)pace.tick(0,paced,visit);assert(paced.attempts==1&&paced.sent.back().topic=="early");
 pace.tick(99,paced,visit);assert(paced.attempts==1);pace.tick(100,paced,visit);assert(paced.sent.back().topic=="late");
 paced.fail=true;pace.tick(10000,paced,visit);const auto tries=paced.attempts;
 pace.tick(10000,paced,visit);assert(paced.attempts==tries);paced.fail=false;
 for(unsigned t=10100;t<10600;t+=100)pace.tick(t,paced,visit);assert(paced.attempts==tries+5);
 telemetry::PacedPublication wrap;wrap.tick(0xffffffd0u,paced,visit);const auto wrapCount=paced.attempts;
 wrap.tick(51,paced,visit);assert(paced.attempts==wrapCount);wrap.tick(52,paced,visit);assert(paced.attempts==wrapCount+1);
 // A scalar changing every loop must not starve cells or the complete scalar refresh.
 telemetry::PacedPublication fair;telemetry::Publisher scalars;telemetry::CellPublisher cells;Wire mixed;
 scalars.setSourceAvailable(telemetry::Source::Bms,true);scalars.onConnected();
 for(size_t i=0;i<telemetry::channelCount;++i)if(telemetry::channels[i].source==telemetry::Source::Bms)scalars.setValue(i,{true,1});
 sample.receivedAt=0;sample.sequence=10;cells.observe(sample,0,0);
 unsigned cellCount=0;
 for(unsigned now=0;now<10000;++now){
  scalars.setValue(3,{true,double(now)});const auto count=mixed.attempts;
  fair.tick(now,mixed,[&](unsigned slot,telemetry::Transport& wire){if(slot==0)scalars.tick(now,wire,1);else if(slot==1)cells.tick(now,"SolarBatteryTelemetry",wire);});
  assert(mixed.attempts-count<=1);
 }
 bool lastField=false;for(const auto& m:mixed.sent){cellCount+=m.topic=="SolarBatteryTelemetry/bms/cells";lastField|=m.topic=="SolarBatteryTelemetry/bms/temperature_2_c";}
 assert(cellCount==1&&lastField&&mixed.attempts<=100);
 telemetry::StatusPublisher status;Wire urgent;status.set(0,"health");status.set(3,"result");
 status.tick(0,"root",urgent,1);assert(urgent.sent.size()==1&&urgent.sent[0].payload=="result");
 status.tick(0,"root",urgent,1);assert(urgent.sent.size()==1);
 status.tick(0,"root",urgent,2);assert(urgent.sent.size()==2&&urgent.sent.back().payload=="health");
 sbt::diagnostics::RuntimePublisher runtime;Wire diag;
 assert(runtime.due(0));runtime.observe(0,5400,40000,6900,38000);
 runtime.tick(0,"SBTTest/COM8",diag);assert(diag.sent.size()==1&&!diag.sent.back().retained);
 assert(diag.sent.back().topic=="SBTTest/COM8/Diagnostics/Runtime");
 assert(diag.sent.back().payload=="{\"uptime_ms\":0,\"loop_stack_min_free_bytes\":5400,\"heap_free_bytes\":40000,\"heap_largest_free_block_bytes\":6900,\"heap_min_free_bytes\":38000}");
 assert(!runtime.due(29999));assert(runtime.due(30000));runtime.observe(30000,5000,39000,6800,37000);
 diag.fail=true;runtime.tick(30000,"SBTTest/COM8",diag);const auto diagAttempts=diag.attempts;
 diag.fail=false;runtime.tick(30249,"SBTTest/COM8",diag);assert(diag.attempts==diagAttempts);
 runtime.tick(30250,"SBTTest/COM8",diag);assert(diag.sent.size()==2);
 // Real six-owner initial/full refresh under continuously changing early scalars.
 telemetry::Publisher full;telemetry::CellPublisher pack;telemetry::StatusPublisher health;
 telemetry::VictronHistoryPublisher history;sbt::diagnostics::PanicSummaryPublisher panic;
 sbt::diagnostics::RuntimePublisher metrics;telemetry::PacedPublication scheduler;Wire all;
 telemetry::Value values[telemetry::channelCount];for(auto& v:values)v={true,1};
 full.setSourceAvailable(telemetry::Source::Bms,true);full.setSourceAvailable(telemetry::Source::Victron,true);
 for(size_t i=0;i<telemetry::channelCount;++i)full.setValue(i,values[i]);
 full.setRefreshInterval(5000);full.onConnected();sample.receivedAt=0;sample.sequence=11;pack.observe(sample,0,0);pack.setRefreshInterval(5000);
 health.set(0,"bms");health.set(1,"victron");health.setRefreshInterval(5000);
 history.observe(values,true,0,0,0);panic.capture({});metrics.observe(0,5400,40000,6900,38000);metrics.setRefreshInterval(5000);
 unsigned firstLast=0,refreshLast=0,firstCell=0,lastCount=0;
 for(unsigned now=0;now<11000;++now){
  full.setValue(3,{true,double(now)});const auto beforeAttempts=all.attempts;const auto beforeMessages=all.sent.size();
  scheduler.tick(now,all,[&](unsigned slot,telemetry::Transport& wire){
   if(slot==0)full.tick(now,wire,1);else if(slot==1)pack.tick(now,"root",wire);
   else if(slot==2)health.tick(now,"root",wire,2);else if(slot==3)history.tick(now,"root",wire);
   else if(slot==4)panic.tick(now,"root",wire);else metrics.tick(now,"root",wire);
  });
  assert(all.attempts-beforeAttempts<=1);
  if(beforeMessages!=all.sent.size()){
   const auto& m=all.sent.back();
   if(m.topic=="root/bms/cells"&&!firstCell)firstCell=now;
   if(m.topic=="SolarBatteryTelemetry/bms/temperature_2_c"){if(lastCount++==0)firstLast=now;else if(!refreshLast)refreshLast=now;}
  }
 }
 assert(firstCell<=1000&&firstLast<=5000&&refreshLast>=5000&&refreshLast<=10000&&lastCount>=2&&all.attempts<=110);
 std::printf("Six-owner timing: initial last scalar %u ms, first cells %u ms, refreshed last scalar %u ms; %u attempts / 11000 ms\n",firstLast,firstCell,refreshLast,all.attempts);
 std::puts("Cell coherent array/delta/retry/stale/refresh/root/clock and paced admission/fairness/rollover passed");
}
