#include "telemetry/VictronHistoryPublisher.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace telemetry;
// [MOCKED!] Only MQTT admission is replaced; history capture and serialization are production code.
struct Wire:Transport {
 struct Message{std::string topic,payload;bool retained;};
 std::vector<Message> sent;bool online=true,fail=false;
 bool connected()const override{return online;}
 bool publish(const char* t,const char* p,bool r)override{if(!online||fail)return false;sent.push_back({t,p,r});return true;}
 void disconnect()override{online=false;}void configure(const char*)override{}
};
static const Wire::Message* find(const Wire& w,const char* suffix){for(const auto& m:w.sent)if(m.topic==std::string("root/victron/last_valid/")+suffix)return &m;return nullptr;}
static void drain(VictronHistoryPublisher& p,Wire& w,uint32_t now){for(int i=0;i<4;++i)p.tick(now,"root",w);}
int main(){
 Value v[channelCount]{};const auto error=channelIndex("victron/error_code"),yield=channelIndex("victron/yield_today_wh");
 VictronHistoryPublisher p;Wire w;drain(p,w,0);assert(w.sent.empty());
 v[error]={true,0};v[yield]={true,1230};
 // Reception age, not publication time, determines the persisted UTC time.
 p.observe(v,true,1000,3500,1792600002);drain(p,w,3500);
 auto e=find(w,"error_code"),y=find(w,"yield_today_wh");
 assert(e&&e->retained&&e->payload=="{\"value\":0,\"received_at\":1792600000}");
 assert(y&&y->retained&&y->payload=="{\"value\":1230,\"received_at\":1792600000}");
 w.sent.clear();
 p.observe(v,true,1000,4000,1792600002);drain(p,w,4000);assert(w.sent.empty());
 // Invalid fields cannot erase or re-date the last valid field from a different sample.
 v[error]={};v[yield]={true,1240};p.observe(v,true,4500,4500,1792600008);drain(p,w,4500);assert(w.sent.empty());
 drain(p,w,8500);assert(!find(w,"error_code"));assert(find(w,"yield_today_wh")->payload=="{\"value\":1240,\"received_at\":1792600008}");
 w.sent.clear();p.observe(v,false,9000,50000,1792600049);p.onConnected();drain(p,w,50000);
 assert(find(w,"error_code")->payload=="{\"value\":0,\"received_at\":1792600000}");
 assert(find(w,"yield_today_wh")->payload=="{\"value\":1240,\"received_at\":1792600008}");
 // An unsynchronized clock is explicit, and merely obtaining a clock does not re-date a sample.
 VictronHistoryPublisher unknown;Wire u;v[error]={true,2};v[yield]={};unknown.observe(v,true,0,0,0);drain(unknown,u,0);
 assert(find(u,"error_code")->payload=="{\"value\":2,\"received_at\":null}");u.sent.clear();
 unknown.observe(v,true,0,5000,1792600000);drain(unknown,u,5000);assert(u.sent.empty());
 unknown.observe(v,true,6000,6000,1792600001);drain(unknown,u,6000);
 assert(find(u,"error_code")->payload=="{\"value\":2,\"received_at\":1792600001}");
 // Retry preserves payload and original receive time, including monotonic wrap.
 VictronHistoryPublisher retry;Wire r;v[error]={true,3};retry.observe(v,true,0xfffffff0u,0x7c0u,1792600002);
 r.fail=true;drain(retry,r,100);assert(r.sent.empty());r.fail=false;drain(retry,r,349);assert(r.sent.empty());drain(retry,r,350);
 assert(find(r,"error_code")->payload=="{\"value\":3,\"received_at\":1792600000}");
 // Reconnect/root changes replay real history without inventing unavailable fields.
 r.sent.clear();retry.onConnected();for(int i=0;i<3;++i)retry.tick(351,"other",r);
 assert(r.sent.size()==1&&r.sent[0].topic=="other/victron/last_valid/error_code");
 // A busy BLE stream must not starve yield or turn the display path into a publish flood.
 VictronHistoryPublisher busy;Wire stream;v[error]={true,0};v[yield]={true,10};
 for(uint32_t i=0;i<100;++i){busy.observe(v,true,i,i,1792600000);busy.tick(i,"root",stream);}
 assert(stream.sent.size()==2&&find(stream,"error_code")&&find(stream,"yield_today_wh"));
 for(uint32_t i=5000;i<5100;++i){busy.observe(v,true,i,i,1792600005);busy.tick(i,"root",stream);}
 assert(stream.sent.size()==4);
 std::puts("Victron retained history: valid fields, original UTC, no-data preservation, clock, retry and wrap passed");
}
