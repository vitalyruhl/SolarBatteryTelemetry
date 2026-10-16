#include "telemetry/StatusPublisher.h"
#include <cassert>
#include <string>
#include <vector>
#include <cstdio>
using namespace telemetry;
// [MOCKED!] Only broker admission; retry and change tracking use production code.
struct Wire:Transport{bool online=true,fail=false;std::vector<std::string> sent;
 bool connected()const override{return online;}bool publish(const char* t,const char* p,bool)override{if(fail||!online)return false;sent.emplace_back(std::string(t)+"="+p);return true;}
 void disconnect()override{online=false;}void configure(const char*)override{}
};
int main(){StatusPublisher p;Wire wire;
 char output[512];assert(formatCommandResult("x","rejected","disabled",nullptr,false,output,sizeof(output)));
 assert(std::strstr(output,"\"charge_enabled\":null")&&std::strstr(output,"\"discharge_enabled\":null"));
 Value observed[channelCount]{};observed[8]={true,1};observed[9]={true,0};observed[4]={true,-2.5};
 assert(formatCommandResult("x","verified","ok",observed,true,output,sizeof(output)));
 assert(std::strstr(output,"\"charge_enabled\":true")&&std::strstr(output,"\"discharge_enabled\":false")&&std::strstr(output,"\"observed_current_a\":-2.5"));
 assert(p.set(0,"{\"available\":true}"));p.tick(0,"root",wire);assert(wire.sent.size()==1);
 p.tick(1,"root",wire);assert(wire.sent.size()==1);
 assert(p.set(0,"{\"available\":false}"));wire.fail=true;p.tick(2,"root",wire);assert(wire.sent.size()==1);
 wire.fail=false;p.tick(3,"root",wire);assert(wire.sent.size()==1);p.tick(252,"root",wire);assert(wire.sent.size()==2&&wire.sent.back().find("false")!=std::string::npos);
 p.onConnected();p.tick(253,"new-root",wire);assert(wire.sent.size()==3&&wire.sent.back().find("new-root/bms/health")==0);
 assert(p.set(3,"verified-old"));p.tick(254,"new-root",wire);wire.sent.clear();
 p.onConnected();for(int i=0;i<4;++i)p.tick(255+i,"new-root",wire);
 for(const auto& message:wire.sent)assert(message.find("verified-old")==std::string::npos);
 // A scheduled health heartbeat must survive payload deduplication, even without samples.
 StatusPublisher heartbeat;Wire heartbeatWire;
 assert(heartbeat.set(0,"unavailable"));heartbeat.tick(0,"root",heartbeatWire);
 assert(heartbeat.set(0,"unavailable"));heartbeat.tick(4999,"root",heartbeatWire);assert(heartbeatWire.sent.size()==1);
 assert(heartbeat.set(0,"unavailable",true));heartbeatWire.fail=true;heartbeat.tick(5000,"root",heartbeatWire);assert(heartbeatWire.sent.size()==1);
 heartbeatWire.fail=false;heartbeat.tick(5249,"root",heartbeatWire);assert(heartbeatWire.sent.size()==1);
 heartbeat.tick(5250,"root",heartbeatWire);assert(heartbeatWire.sent.size()==2&&heartbeatWire.sent.back()=="root/bms/health=unavailable");
 assert(!p.set(9,"bad"));std::string big(600,'x');assert(!p.set(0,big.c_str()));
 Publisher values;values.setControl(true,true);values.onConnected();for(int i=0;i<20;++i)values.tick(0,wire);
 bool ready=false,enabled=false;for(const auto& v:wire.sent){ready|=v=="SolarBatteryTelemetry/bms/control/availability=available";enabled|=v.find("\"commands_enabled\":true")!=std::string::npos;}
 assert(ready&&enabled);wire.sent.clear();values.setControl(true,false);for(int i=0;i<20;++i)values.tick(1,wire);
 bool unavailable=false;for(const auto& v:wire.sent)unavailable|=v=="SolarBatteryTelemetry/bms/control/availability=unavailable";assert(unavailable);
 std::puts("Independent source/control status, failure retry and immediate readiness changes passed");
}
