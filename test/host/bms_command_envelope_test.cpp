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
 // Explicit manual single-target commands may carry a peer-bound confirmed baseline.
 session.begin("0123456789abcdef0123456789abcdef");
 const char* cached=R"({"state":false,"baseline_charge_enabled":true,"baseline_discharge_enabled":false,"peer":"AA:BB:CC:DD:EE:FF","request_id":"cached-1","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})";
 assert(!std::strcmp(session.accept("bms/charge_enabled/set",cached,std::strlen(cached),1800000000,100,result),"ok"));
 assert(result.target==bms::MosTarget::Charge&&result.baselineKnown&&result.baselineMask==1&&!std::strcmp(result.peer,"aa:bb:cc:dd:ee:ff"));
 for(const auto& field:{std::string("\"baseline_charge_enabled\":true"),std::string("\"baseline_discharge_enabled\":false"),std::string("\"peer\":\"AA:BB:CC:DD:EE:FF\"")}){
  auto invalid=std::string(cached);auto pos=invalid.find(field);invalid.replace(pos,field.size(),field.substr(0,field.find(':')+1)+"null");
  session.begin("0123456789abcdef0123456789abcdef");assert(std::strcmp(session.accept("bms/charge_enabled/set",invalid.data(),invalid.size(),1800000000,100,result),"ok"));
 }
 // Joint permission initialization must use its exact five-field envelope.
 session.begin("0123456789abcdef0123456789abcdef");
 const char* both=R"({"charge_enabled":true,"discharge_enabled":false,"request_id":"both-1","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})";
 assert(!std::strcmp(session.accept("bms/permissions/set",both,std::strlen(both),1800000000,100,result),"ok"));
 assert(result.target==bms::MosTarget::Both&&result.state&&!result.dischargeState&&result.deadline==20100);
 assert(!std::strcmp(session.accept("bms/permissions/set",both,std::strlen(both),1800000000,100,result),"duplicate"));
 // IDs share one replay ledger across single and joint commands.
 const char* collision=R"({"state":false,"request_id":"both-1","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})";
 assert(!std::strcmp(session.accept("bms/charge_enabled/set",collision,std::strlen(collision),1800000000,100,result),"duplicate"));
 session.begin("0123456789abcdef0123456789abcdef");
 assert(!std::strcmp(session.accept("bms/permissions/set",both,std::strlen(both),0,100,result),"clock_unavailable"));
 assert(!std::strcmp(session.accept("bms/permissions/set",both,std::strlen(both),1800000020,100,result),"expired"));
 assert(!std::strcmp(session.accept("bms/permissions/set",both,std::strlen(both),1799999989,100,result),"expiry_too_far"));
 assert(!std::strcmp(session.accept("bms/permissions/set",both,std::strlen(both),1799999990,0xfffffff0u,result),"ok"));
 assert(result.deadline==29984u);
 session.begin("1123456789abcdef0123456789abcdef");
 assert(!std::strcmp(session.accept("bms/permissions/set",both,std::strlen(both),1800000000,100,result),"old_session"));
 for(bool charge:{false,true})for(bool discharge:{false,true}){
  session.begin("0123456789abcdef0123456789abcdef");
  char payload[256];std::snprintf(payload,sizeof(payload),"{\"charge_enabled\":%s,\"discharge_enabled\":%s,\"request_id\":\"pair\",\"session\":\"0123456789abcdef0123456789abcdef\",\"expires_at\":1800000020}",charge?"true":"false",discharge?"true":"false");
  assert(!std::strcmp(session.accept("bms/permissions/set",payload,std::strlen(payload),1800000000,100,result),"ok"));
  assert(result.target==bms::MosTarget::Both&&result.state==charge&&result.dischargeState==discharge);
 }
 const char* jointBad[]={
 R"({"state":false,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})",
 R"({"charge_enabled":true,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})",
 R"({"discharge_enabled":false,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})",
 R"({"charge_enabled":"true","discharge_enabled":false,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})",
 R"({"charge_enabled":true,"discharge_enabled":0,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})",
 R"({"charge_enabled":true,"discharge_enabled":false,"state":false,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})",
 R"({"charge_enabled":true,"charge_enabled":false,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})",
 R"({"charge_enabled":true,"discharge_enabled":false,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020}{})",
 R"({"charge_enabled":true,"discharge_enabled":false,"request_id":"a\u0000x","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})"};
 for(auto bad:jointBad){
  session.begin("0123456789abcdef0123456789abcdef");
  assert(!std::strcmp(session.accept("bms/permissions/set",bad,std::strlen(bad),1800000000,100,result),"invalid_envelope"));
 }
 // A joint payload and added fields must not weaken the original single-command schema.
 for(const char* suffix:{"bms/charge_enabled/set","bms/discharge_enabled/set"}){
  assert(!std::strcmp(session.accept(suffix,both,std::strlen(both),1800000000,100,result),"invalid_envelope"));
  const char* mixed=R"({"state":false,"charge_enabled":true,"request_id":"a","session":"0123456789abcdef0123456789abcdef","expires_at":1800000020})";
  assert(!std::strcmp(session.accept(suffix,mixed,std::strlen(mixed),1800000000,100,result),"invalid_envelope"));
 }
 session.begin("0123456789abcdef0123456789abcdef");
 for(unsigned i=0;i<16;++i){char payload[256];std::snprintf(payload,sizeof(payload),"{\"charge_enabled\":false,\"discharge_enabled\":true,\"request_id\":\"%u\",\"session\":\"0123456789abcdef0123456789abcdef\",\"expires_at\":1800000020}",i);
  assert(!std::strcmp(session.accept("bms/permissions/set",payload,std::strlen(payload),1800000000,100,result),"ok"));
 }
 assert(!std::strcmp(session.accept("bms/permissions/set",both,std::strlen(both),1800000000,100,result),"session_exhausted"));
 // Gate settings bypass physical readiness later; the envelope still enforces session/expiry.
 session.begin("0123456789abcdef0123456789abcdef");
 assert(!std::strcmp(session.accept("bms/control_enabled/set",valid,std::strlen(valid),1800000000,100,result),"ok"));
 assert(result.target==bms::MosTarget::Gate&&!result.state);
 session.begin("0123456789abcdef0123456789abcdef");
 assert(!std::strcmp(session.accept("bms/emergency_off/set",valid,std::strlen(valid),1800000000,100,result),"ok"));
 assert(result.emergency&&result.target==bms::MosTarget::Both&&!result.state&&!result.dischargeState);
 std::string unsafe(valid);unsafe.replace(unsafe.find("false"),5,"true");session.begin("0123456789abcdef0123456789abcdef");
 assert(!std::strcmp(session.accept("bms/emergency_off/set",unsafe.data(),unsafe.size(),1800000000,100,result),"emergency_off_only"));
 std::puts("MQTT command allowlist, session, replay, expiry and bounds passed");
}
