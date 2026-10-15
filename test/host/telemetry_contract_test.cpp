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
    std::string will; int failAt=-1,attempts=0;
    bool connected() const override {return online;}
    bool publish(const char* t,const char* p,bool r) override {
        ++attempts;
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
    assert(w.sent.size()==5&&w.sent[0].topic=="SolarBatteryTelemetry/bms/availability");
    for(const auto* leaf:expected) {std::string path=std::string("SolarBatteryTelemetry/")+leaf;const auto* m=find(w,path.c_str());assert(!m);}
    auto sent=w.sent.size();drain(p,w,5249);assert(w.sent.size()==sent);
    drain(p,w,5250);assert(w.sent.size()==sent);
    w.sent.clear();p.onConnected();drain(p,w,5251);assert(w.sent.size()==5);
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
    w.online=true;p.onConnected();drain(p,w,6200);assert(!find(w,"latest/bms/voltage_v")&&find(w,"latest/status"));
    Wire wrap;Publisher q;q.setValue(3,Value{true,1});q.setSourceAvailable(Source::Bms,true);q.onConnected();drain(q,wrap,0xfffffff0u);auto n=wrap.sent.size();q.setValue(3,Value{true,2});
    drain(q,wrap,0x00001377u);assert(wrap.sent.size()==n);drain(q,wrap,0x00001378u);assert(wrap.sent.size()==n+1);
    Wire paused;Publisher r;r.onConnected();r.setPaused(true);drain(r,paused,100);assert(paused.sent.empty());
    r.setPaused(false);drain(r,paused,101);assert(paused.sent.size()==5);
    Wire partial;Publisher partialPolicy;partialPolicy.onConnected();partial.failAt=3;
    drain(partialPolicy,partial,0);assert(partial.sent.size()==3&&!partialPolicy.delivered());
    partial.failAt=-1;drain(partialPolicy,partial,250);assert(partial.sent.size()==5&&partialPolicy.delivered());
    partial.sent.clear();assert(partialPolicy.requestRoot("pending",7));partial.fail=true;partialPolicy.tick(500,partial);
    assert(partialPolicy.requestRoot("newest",6));partialPolicy.tick(750,partial);assert(partial.will.empty());
    partial.fail=false;partialPolicy.tick(1000,partial);assert(partial.will=="newest/status");
    Wire never;never.online=false;Publisher noHistory;
    assert(noHistory.requestRoot("unused",6));noHistory.tick(0,never);
    assert(noHistory.requestRoot("actual",6));noHistory.tick(1,never);
    never.online=true;noHistory.onConnected();drain(noHistory,never,2);
    assert(never.sent.size()==5&&never.sent[0].topic=="actual/bms/availability");
    Wire limited;Publisher oneTick;oneTick.onConnected();oneTick.tick(0,limited);assert(limited.sent.size()==4);
    oneTick.setPaused(true);assert(oneTick.requestRoot("during-ota",10));oneTick.tick(1,limited);assert(limited.will.empty());
    assert(oneTick.requestRoot("latest-ota",10));oneTick.setPaused(false);oneTick.tick(2,limited);assert(limited.will=="latest-ota/status");
    // [MOCKED!] A decoded solar sample is supplied through the production publisher API.
    Wire solar; Publisher solarPublisher;
    solarPublisher.setValue(15, Value{true,128});
    solarPublisher.setSourceAvailable(Source::Victron,true);
    solarPublisher.onConnected();drain(solarPublisher,solar,0);
    assert(find(solar,"SolarBatteryTelemetry/victron/availability")->payload=="available");
    assert(find(solar,"SolarBatteryTelemetry/victron/solar_power_w")->payload=="128");
    assert(find(solar,"SolarBatteryTelemetry/bms/availability")->payload=="unavailable");
    solar.sent.clear();solarPublisher.setSourceAvailable(Source::Victron,false);
    drain(solarPublisher,solar,1);
    assert(find(solar,"SolarBatteryTelemetry/victron/availability")->payload=="unavailable");
    assert(!find(solar,"SolarBatteryTelemetry/victron/solar_power_w"));
    // A freshness transition during a partial send restarts with coherent availability/data.
    Wire interrupted;Publisher transition;transition.setValue(15,Value{true,20});
    transition.setSourceAvailable(Source::Victron,true);transition.onConnected();transition.tick(0,interrupted);
    interrupted.sent.clear();transition.setSourceAvailable(Source::Victron,false);drain(transition,interrupted,1);
    assert(find(interrupted,"SolarBatteryTelemetry/victron/availability")->payload=="unavailable");
    assert(!find(interrupted,"SolarBatteryTelemetry/victron/solar_power_w"));
    // [MOCKED!] Source values exercise delta policy and wire-level equality.
    Wire delta;Publisher changed;
    changed.setValue(15,Value{true,128});changed.setValue(18,Value{true,0});
    changed.setSourceAvailable(Source::Victron,true);changed.onConnected();drain(changed,delta,0);
    assert(delta.sent.size()==7&&find(delta,"SolarBatteryTelemetry/victron/battery_voltage_v")->payload=="0");
    delta.sent.clear();changed.setValue(15,Value{true,128.0000001});drain(changed,delta,5000);assert(delta.sent.empty());
    changed.setValue(15,Value{true,129});drain(changed,delta,9999);assert(delta.sent.empty());
    drain(changed,delta,10000);assert(delta.sent.size()==1&&delta.sent[0].payload=="129"&&!delta.sent[0].retain);
    delta.sent.clear();changed.setValue(18,Value{});drain(changed,delta,15000);
    assert(delta.sent.size()==1&&delta.sent[0].payload=="null");
    delta.sent.clear();drain(changed,delta,20000);assert(delta.sent.empty());
    changed.setValue(18,Value{true,0});drain(changed,delta,25000);assert(delta.sent.size()==1&&delta.sent[0].payload=="0");
    delta.sent.clear();changed.setSourceAvailable(Source::Victron,false);drain(changed,delta,25001);
    assert(delta.sent.size()==2&&find(delta,"SolarBatteryTelemetry/victron/availability")->payload=="unavailable");
    assert(find(delta,"SolarBatteryTelemetry/capabilities")&&!find(delta,"SolarBatteryTelemetry/victron/battery_voltage_v"));
    delta.sent.clear();drain(changed,delta,30001);assert(delta.sent.empty());
    changed.setValue(15,Value{true,129});changed.setValue(18,Value{true,0});changed.setSourceAvailable(Source::Victron,true);
    drain(changed,delta,30002);assert(delta.sent.size()==4&&find(delta,"SolarBatteryTelemetry/victron/solar_power_w"));
    delta.sent.clear();changed.onConnected();drain(changed,delta,30003);assert(delta.sent.size()==7);
    delta.sent.clear();changed.requestRoot("delta-new",9);changed.tick(30004,delta);delta.online=true;changed.onConnected();delta.sent.clear();
    drain(changed,delta,30005);assert(delta.sent.size()==7&&find(delta,"delta-new/victron/battery_voltage_v")->payload=="0");
    // A failed field admission neither advances its cache nor retries before 250 ms.
    Wire failures;Publisher retried;retried.setValue(3,Value{true,1});retried.setSourceAvailable(Source::Bms,true);retried.onConnected();drain(retried,failures,0);
    failures.sent.clear();retried.setValue(3,Value{true,2});failures.fail=true;drain(retried,failures,5000);assert(failures.sent.empty()&&!retried.delivered());
    failures.fail=false;drain(retried,failures,5249);assert(failures.sent.empty());drain(retried,failures,5250);
    assert(failures.sent.size()==1&&failures.sent[0].payload=="2");failures.sent.clear();drain(retried,failures,10250);assert(failures.sent.empty());
    retried.setValue(3,Value{true,1});drain(retried,failures,15250);assert(failures.sent.size()==1&&failures.sent[0].payload=="1");
    // A snapshot remains coherent when live values change halfway through its metadata.
    Wire coherent;Publisher frozen;frozen.setValue(3,Value{true,48});frozen.setSourceAvailable(Source::Bms,true);frozen.onConnected();frozen.tick(0,coherent);
    frozen.setValue(3,Value{true,49});drain(frozen,coherent,0);assert(find(coherent,"SolarBatteryTelemetry/bms/voltage_v")->payload=="48");
    coherent.sent.clear();drain(frozen,coherent,5000);assert(coherent.sent.size()==1&&coherent.sent[0].payload=="49");
    // A partial delta keeps accepted fields, even when another source restarts the cycle.
    Wire partDelta;Publisher accepted;
    accepted.setValue(3,Value{true,10});accepted.setValue(4,Value{true,20});accepted.setSourceAvailable(Source::Bms,true);
    accepted.onConnected();drain(accepted,partDelta,0);partDelta.sent.clear();
    accepted.setValue(3,Value{true,11});accepted.setValue(4,Value{true,21});partDelta.failAt=1;
    drain(accepted,partDelta,5000);assert(partDelta.sent.size()==1&&partDelta.sent[0].payload=="11");
    const auto attempts=partDelta.attempts;
    accepted.setValue(15,Value{true,30});accepted.setSourceAvailable(Source::Victron,true);
    partDelta.failAt=-1;drain(accepted,partDelta,5249);assert(partDelta.attempts==attempts);
    drain(accepted,partDelta,5250);assert(partDelta.sent.size()==5);
    unsigned voltageWrites=0;for(const auto& message:partDelta.sent)if(message.topic=="SolarBatteryTelemetry/bms/voltage_v")++voltageWrites;
    assert(voltageWrites==1&&find(partDelta,"SolarBatteryTelemetry/bms/current_a")->payload=="21");
    partDelta.sent.clear();drain(accepted,partDelta,10250);assert(partDelta.sent.empty());
    // Reconnect abandons an unfinished snapshot and replays current, coherent values.
    Wire reconnect;Publisher fresh;
    fresh.setValue(3,Value{true,40});fresh.setSourceAvailable(Source::Bms,true);fresh.onConnected();fresh.tick(0,reconnect);
    fresh.setValue(3,Value{true,41});fresh.onConnected();reconnect.sent.clear();drain(fresh,reconnect,1);
    assert(reconnect.sent.size()==6&&find(reconnect,"SolarBatteryTelemetry/bms/voltage_v")->payload=="41");
    // The cursor budget also bounds attempts when every supported field needs publication.
    Wire bounded;Publisher all;
    for(std::size_t i=0;i<channelCount;++i)all.setValue(i,Value{true,1});
    all.setSourceAvailable(Source::Bms,true);all.setSourceAvailable(Source::Victron,true);all.onConnected();
    for(int tick=0;tick<20;++tick){const auto before=bounded.attempts;all.tick(0,bounded);assert(bounded.attempts-before<=4);}
    assert(bounded.sent.size()==27&&all.delivered());
    bounded.sent.clear();drain(all,bounded,5000);assert(bounded.sent.empty());
    std::puts("Telemetry contract: bounds, serialized deltas, absent sources, partial retries, reconnect/rename, wrap and OTA pause passed");
}
