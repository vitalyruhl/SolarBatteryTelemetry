#include "telemetry/MqttLoopPolicy.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace telemetry;
// [MOCKED!] Network send/connection boundary; production retirement state machine remains real.
struct Wire:Transport {
 bool online=true,fail=false;bool* pause=nullptr;bool pauseAfterSend=false;std::vector<std::string> sent;
 bool connected()const override{return online;}
 bool publish(const char*t,const char*p,bool)override{if(!online||fail||(pause&&*pause))return false;sent.push_back(std::string(t)+"="+p);if(pauseAfterSend&&pause)*pause=true;return true;}
 void disconnect()override{online=false;}
 void configure(const char*)override{}
 bool contains(const char* s)const {for(const auto&m:sent)if(m==s)return true;return false;}
};
int main(){int failures=0;
 {Publisher p;Wire w;p.onConnected();for(int i=0;i<10;++i)p.tick(0,w);
  w.fail=true;p.retireCurrent(w);w.disconnect();w.fail=false;w.online=true;p.onConnected();
  for(int i=0;i<10;++i)p.tick(250,w);w.sent.clear();
  bool retired=p.retireCurrent(w);w.disconnect();
  if(!retired||!w.contains("SolarBatteryTelemetry/status=offline")){++failures;std::puts("FAIL: same-root reconnect then disable leaves online");}
 }
 {Publisher p;Wire w;p.onConnected();for(int i=0;i<10;++i)p.tick(0,w);
  w.online=false;p.requestRoot("next",4);p.tick(1,w);
  w.online=true;p.onConnected();w.publish("next/status","online",true);w.sent.clear();
  bool retired=p.retireCurrent(w);w.disconnect();
  if(!retired||!w.contains("SolarBatteryTelemetry/status=offline")||!w.contains("next/status=offline")){++failures;std::puts("FAIL: disable with old cleanup skips current online root");}
 }
 {Publisher p;Wire w;p.onConnected();p.tick(0,w);w.fail=true;
  const auto cleanup=disconnectForReconfigure(p,w,true);
  assert(cleanup==CleanupResult::UnconfirmedOldEndpoint&&!p.retirementPending()&&!w.online);
  w.fail=false;w.online=true;p.onConnected();w.sent.clear();for(int i=0;i<10;++i)p.tick(250,w);
  assert(!w.contains("SolarBatteryTelemetry/status=offline"));
  assert(p.retireCurrent(w)&&w.contains("SolarBatteryTelemetry/status=offline"));
 }
 {Publisher p;Wire w;p.onConnected();p.tick(0,w);w.fail=true;
  assert(disconnectForReconfigure(p,w,false)==CleanupResult::DeferredSameEndpoint&&p.retirementPending());
  w.fail=false;w.online=true;p.onConnected();for(int i=0;i<10;++i)p.tick(250,w);
  w.sent.clear();assert(disconnectForReconfigure(p,w,false)==CleanupResult::Retired&&w.contains("SolarBatteryTelemetry/status=offline"));
 }
 {Publisher p;Wire w;bool paused=false;int networkCalls=0;p.onConnected();w.pause=&paused;w.pauseAfterSend=true;
  runPreparedMqttLoop(p,0,w,true,[&](){return paused;},[&](){++networkCalls;});
  assert(paused&&networkCalls==0&&w.sent.size()==1);
  runPreparedMqttLoop(p,1,w,true,[&](){return paused;},[&](){++networkCalls;});assert(networkCalls==0&&w.sent.size()==1);
  paused=false;w.pauseAfterSend=false;runPreparedMqttLoop(p,250,w,true,[&](){return paused;},[&](){++networkCalls;});assert(networkCalls==1);
 }
 if(!failures)std::puts("MQTT lifecycle: same-root reconnect/disable, dual retirement, endpoint ownership and OTA boundary passed");
 return failures?1:0;
}
