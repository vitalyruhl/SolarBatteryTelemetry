#include "telemetry/TelemetryChannels.h"
#include "telemetry/TelemetryPublisher.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
using namespace telemetry;
// [MOCKED!] Network admission boundary only; all policy/serialization is production code.
struct Wire : Transport {
    struct Message { std::string topic,payload; bool retain; };
    bool online=true, fail=false; int disconnects=0; std::vector<Message> sent;
    std::string will; int failAt=-1;
    bool connected() const override {return online;}
    bool publish(const char* t,const char* p,bool r) override {
        if(!online||fail||(failAt>=0&&static_cast<int>(sent.size())==failAt))return false; sent.push_back({t,p,r}); return true;
    }
    void disconnect() override {online=false;++disconnects;}
    void configure(const char* root) override {will=std::string(root)+"/status";}
};
static void drain(Publisher& p,Wire& w,uint32_t now) {for(int n=0;n<20;++n)p.tick(now,w);}
static const Wire::Message* find(const Wire& w,const char* topic) {
    for(const auto& m:w.sent)if(m.topic==topic)return &m;return nullptr;
}
int main() {
    assert(validRoot("SolarBatteryTelemetry",21));
    std::string maxRoot(63,'r'),overRoot(64,'r');
    assert(validRoot(maxRoot.c_str(),63));assert(!validRoot(overRoot.c_str(),64));
    for(const char* r:{"","/root","root/","a//b","a/+","a/#","a\nb","a b","a\"b"})assert(!validRoot(r,std::strlen(r)));
    assert(validRoot("site/collector-1",16));
    char topic[129],payload[1025];
    assert(buildTopic("site/collector-1","bms/voltage_v",topic,sizeof(topic)));
    assert(std::strcmp(topic,"site/collector-1/bms/voltage_v")==0);
    assert(!buildTopic(overRoot.c_str(),"status",topic,sizeof(topic)));
    assert(!buildTopic("x","status",topic,8));
    std::string suffix(64,'s');assert(buildTopic(maxRoot.c_str(),suffix.c_str(),topic,sizeof(topic)));
    suffix+='s';assert(!buildTopic(maxRoot.c_str(),suffix.c_str(),topic,sizeof(topic)));
    const char* expected[]={"bms/charging","bms/discharging","bms/state_of_charge_pct","bms/voltage_v","bms/current_a","bms/power_w","bms/remaining_capacity_ah","bms/full_capacity_ah","bms/charge_enabled","bms/discharge_enabled","bms/temperature_max_c","bms/cell_voltage_min_v","bms/cell_voltage_max_v","bms/cell_voltage_delta_v","bms/alarm_bits","victron/solar_power_w","victron/pv_voltage_v","victron/pv_current_a","victron/battery_voltage_v","victron/charge_current_a","victron/battery_power_w","victron/load_current_a","victron/charge_state","victron/error_code","victron/yield_today_wh"};
    assert(channelCount==25);
    for(std::size_t i=0;i<channelCount;++i) {
        assert(std::strcmp(channels[i].suffix,expected[i])==0);
        assert(channels[i].label[0]);
        for(std::size_t j=0;j<i;++j)assert(std::strcmp(channels[j].suffix,channels[i].suffix));
        assert(formatValue(channels[i],Value{},payload,sizeof(payload))&&std::strcmp(payload,"null")==0);
    }
    assert(channels[0].type==FieldType::Boolean&&channels[14].type==FieldType::Unsigned);
    assert(std::strcmp(channels[6].unit,"Ah")==0&&std::strcmp(channels[19].unit,"A")==0);
    assert(formatValue(channels[0],Value{true,0},payload,sizeof(payload))&&std::strcmp(payload,"false")==0);
    assert(formatValue(channels[0],Value{true,1},payload,sizeof(payload))&&std::strcmp(payload,"true")==0);
    assert(formatValue(channels[3],Value{true,0},payload,sizeof(payload))&&std::strcmp(payload,"0")==0);
    assert(formatValue(channels[3],Value{true,std::numeric_limits<double>::infinity()},payload,sizeof(payload))&&std::strcmp(payload,"null")==0);
    assert(formatValue(channels[14],Value{true,-1},payload,sizeof(payload))&&std::strcmp(payload,"null")==0);
    assert(formatValue(channels[22],Value{true,2},payload,sizeof(payload))&&std::strcmp(payload,"\"bulk\"")==0);
    assert(formatValue(channels[22],Value{true,99},payload,sizeof(payload))&&std::strcmp(payload,"null")==0);
    assert(formatValue(channels[16],Value{true,48},payload,sizeof(payload))&&std::strcmp(payload,"null")==0);
    assert(capabilities("SolarBatteryTelemetry",payload,sizeof(payload)));
    assert(std::strstr(payload,"SolarBatteryTelemetry/bms/charge_enabled/set"));
    assert(std::strstr(payload,"SolarBatteryTelemetry/bms/discharge_enabled/set"));
    assert(std::strstr(payload,"\"read_only\":true")&&std::strstr(payload,"\"commands_enabled\":false"));
    assert(std::strstr(payload,"equalization")&&std::strstr(payload,"pv_current_a"));
    assert(!capabilities("a",payload,16));
    Wire w;Publisher p;assert(p.requestRoot("SolarBatteryTelemetry",21));p.onConnected();
    w.fail=true;drain(p,w,0);assert(w.sent.empty()&&!p.delivered());
    w.fail=false;drain(p,w,250);assert(p.delivered());
    assert(w.sent.size()==30&&w.sent[0].topic=="SolarBatteryTelemetry/bms/availability");
    for(const auto* leaf:expected) {std::string path=std::string("SolarBatteryTelemetry/")+leaf;const auto* m=find(w,path.c_str());assert(m&&m->payload=="null"&&!m->retain);}
    auto sent=w.sent.size();drain(p,w,5249);assert(w.sent.size()==sent);
    drain(p,w,5250);assert(w.sent.size()==sent+30);
    w.sent.clear();p.onConnected();drain(p,w,5251);assert(w.sent.size()==30);
    assert(!p.requestRoot("bad/#",5));assert(std::strcmp(p.root(),"SolarBatteryTelemetry")==0&&p.rootRejected());
    assert(p.requestRoot("new/base",8));w.sent.clear();p.tick(5300,w);
    assert(w.disconnects==1&&w.will=="new/base/status");
    assert(w.sent.size()==3&&w.sent.back().topic=="SolarBatteryTelemetry/status"&&w.sent.back().payload=="offline");
    w.online=true;p.onConnected();drain(p,w,5500);
    for(std::size_t i=3;i<w.sent.size();++i)assert(w.sent[i].topic.find("new/base/")==0);
    // A disconnected rename keeps a bounded old-root retirement obligation through reconnect.
    w.online=false;assert(p.requestRoot("third",5));p.tick(5600,w);assert(w.will=="third/status");
    assert(p.requestRoot("fourth",6));assert(p.requestRoot("latest",6));p.tick(5601,w);assert(w.will=="third/status");
    w.online=true;p.onConnected();w.sent.clear();w.fail=true;p.tick(5700,w);assert(w.sent.empty());
    w.fail=false;drain(p,w,5950);assert(w.sent[0].topic=="new/base/bms/availability");
    assert(w.sent[2].topic=="new/base/status"&&w.sent[3].topic=="third/bms/availability");
    assert(w.will=="latest/status"&&std::strcmp(p.root(),"latest")==0);
    w.online=true;p.onConnected();drain(p,w,6200);assert(find(w,"latest/bms/voltage_v"));
    Wire wrap;Publisher q;q.onConnected();drain(q,wrap,0xfffffff0u);auto n=wrap.sent.size();
    drain(q,wrap,0x00001377u);assert(wrap.sent.size()==n);drain(q,wrap,0x00001378u);assert(wrap.sent.size()==n+30);
    Wire paused;Publisher r;r.onConnected();r.setPaused(true);drain(r,paused,100);assert(paused.sent.empty());
    r.setPaused(false);drain(r,paused,101);assert(paused.sent.size()==30);
    Wire partial;Publisher partialPolicy;partialPolicy.onConnected();partial.failAt=7;
    drain(partialPolicy,partial,0);assert(partial.sent.size()==7&&!partialPolicy.delivered());
    partial.failAt=-1;drain(partialPolicy,partial,250);assert(partial.sent.size()==30&&partialPolicy.delivered());
    partial.sent.clear();assert(partialPolicy.requestRoot("pending",7));partial.fail=true;partialPolicy.tick(500,partial);
    assert(partialPolicy.requestRoot("newest",6));partialPolicy.tick(750,partial);assert(partial.will.empty());
    partial.fail=false;partialPolicy.tick(1000,partial);assert(partial.will=="newest/status");
    Wire never;never.online=false;Publisher noHistory;
    assert(noHistory.requestRoot("unused",6));noHistory.tick(0,never);
    assert(noHistory.requestRoot("actual",6));noHistory.tick(1,never);
    never.online=true;noHistory.onConnected();drain(noHistory,never,2);
    assert(never.sent.size()==30&&never.sent[0].topic=="actual/bms/availability");
    Wire limited;Publisher oneTick;oneTick.onConnected();oneTick.tick(0,limited);assert(limited.sent.size()==4);
    oneTick.setPaused(true);assert(oneTick.requestRoot("during-ota",10));oneTick.tick(1,limited);assert(limited.will.empty());
    assert(oneTick.requestRoot("latest-ota",10));oneTick.setPaused(false);oneTick.tick(2,limited);assert(limited.will=="latest-ota/status");
    std::puts("Telemetry contract: root bounds, 25 channels, validity, retry/reconnect/rename, wrap, OTA pause passed");
}
