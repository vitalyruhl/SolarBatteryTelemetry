#include "telemetry/BmsCommandEnvelope.h"
#include <cassert>
#include <cstdio>
#include <string>
using namespace telemetry;
int main(){
 CommandSession session;session.begin("0123456789abcdef0123456789abcdef");bms::MosRequest result;
 const char* valid=R"({"state":false,"request_id":"a-1","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})";
 assert(!std::strcmp(session.accept("bms/charge_enabled/set",valid,std::strlen(valid),1800000000,100,result),"ok"));
 assert(!result.state&&result.target==bms::MosTarget::Charge&&result.deadline==20100);
 assert(!std::strcmp(session.accept("bms/charge_enabled/set",valid,std::strlen(valid),1800000000,100,result),"duplicate"));
 session.begin("1123456789abcdef0123456789abcdef");assert(!std::strcmp(session.accept("bms/charge_enabled/set",valid,std::strlen(valid),1800000000,100,result),"old_session"));
 session.begin("0123456789abcdef0123456789abcdef");
 assert(!std::strcmp(session.accept("bms/charge_enabled/set",valid,std::strlen(valid),0,100,result),"clock_unavailable"));
 assert(!std::strcmp(session.accept("bms/reset/set",valid,std::strlen(valid),1800000000,100,result),"unsupported_command"));
 assert(!std::strcmp(session.accept("bms/charge_enabled/set",valid,std::strlen(valid),1800000020,100,result),"expired"));
 assert(!std::strcmp(session.accept("bms/charge_enabled/set",valid,std::strlen(valid),1799999980,100,result),"expiry_too_far"));
 for(const char* bad:{"{}","null","[]",R"({"state":"false","request_id":"x","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})",R"({"state":false,"request_id":"bad/id","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})"}){
  assert(std::strcmp(session.accept("bms/charge_enabled/set",bad,std::strlen(bad),1800000000,100,result),"ok"));
 }
 const char* strictBad[]={
 R"({"state":false,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020}garbage)",
 R"({"state":false,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020}{})",
 R"({"state":false,"request_id":"abc\u0000!","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})",
 R"({"state":false,"request_id":"a","session":"0123456789abcdef0123456789abcdef\u0000BAD","expires_at":1800000020})",
 R"({"state":true,"state":false,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})"};
 for(auto bad:strictBad){session.begin("0123456789abcdef0123456789abcdef");assert(std::strcmp(session.accept("bms/charge_enabled/set",bad,std::strlen(bad),1800000000,100,result),"ok"));}
 session.begin("0123456789abcdef0123456789abcdef");
 std::string large(257,' ');assert(!std::strcmp(session.accept("bms/charge_enabled/set",large.c_str(),large.size(),1800000000,100,result),"payload_size"));
 for(unsigned i=0;i<16;++i){char payload[256];std::snprintf(payload,sizeof(payload),"{\"state\":true,\"request_id\":\"%u\",\"session\":\"0123456789abcdef0123456789abcdef\",\"expires_at\":1800000020}",i);
 assert(!std::strcmp(session.accept("bms/discharge_enabled/set",payload,std::strlen(payload),1800000000,100,result),"ok"));}
 assert(!std::strcmp(session.accept("bms/charge_enabled/set",valid,std::strlen(valid),1800000000,100,result),"session_exhausted"));
 session.clear();assert(std::strcmp(session.accept("bms/charge_enabled/set",valid,std::strlen(valid),1800000000,100,result),"ok"));
 std::puts("MQTT command allowlist, session, replay, expiry and bounds passed");
}
