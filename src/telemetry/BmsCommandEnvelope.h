#pragma once
#include <ArduinoJson.h>
#include "bms/BmsCommandPolicy.h"
namespace telemetry {
// Per-connection replay ledger. A full ledger requires a new advertised session.
class CommandSession {
 char token_[33]{};char ids_[16][33]{};size_t count_=0;
 static bool validId(const char* id){
  if(!id)return false;const auto n=std::strlen(id);if(!n||n>32)return false;
  for(size_t i=0;i<n;++i){char c=id[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;}return true;
 }
 // The command vocabulary needs no JSON escapes or nested objects. Reject ambiguous
 // encodings before ArduinoJson can stop early or collapse duplicate properties.
 static bool exactObject(const char* text,size_t size){
  size_t first=0,last=size;auto space=[](char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};
  while(first<last&&space(text[first]))++first;while(last>first&&space(text[last-1]))--last;
  if(last-first<2||text[first]!='{'||text[last-1]!='}')return false;
  bool quoted=false;unsigned properties=0;
  for(size_t i=first+1;i+1<last;++i){const char c=text[i];
   if(c=='\\'||c==0)return false;
   if(c=='"'){quoted=!quoted;continue;}
   if(!quoted){if(c=='{'||c=='}'||c=='['||c==']')return false;if(c==':')++properties;}
  }
  return !quoted&&properties==4;
 }
public:
 void clear(){token_[0]=0;count_=0;}
 void begin(const char* token){clear();if(token&&std::strlen(token)==32)std::memcpy(token_,token,33);}
 const char* token()const{return token_;}
 const char* accept(const char* suffix,const char* payload,size_t length,uint64_t utc,uint32_t now,bms::MosRequest& output){
  if(!suffix||(std::strcmp(suffix,"bms/charge_enabled/set")&&std::strcmp(suffix,"bms/discharge_enabled/set")))return "unsupported_command";
  if(!payload||!length||length>256)return "payload_size";
  if(utc<1577836800)return "clock_unavailable";
  if(!exactObject(payload,length))return "invalid_envelope";
  JsonDocument doc;auto error=deserializeJson(doc,payload,length);
  if(error||!doc.is<JsonObject>()||doc.size()!=4||!doc["state"].is<bool>()||!doc["expires_at"].is<uint64_t>()||!doc["request_id"].is<const char*>()||!doc["session"].is<const char*>())return "invalid_envelope";
  const char* id=doc["request_id"].as<const char*>();const char* token=doc["session"].as<const char*>();
  if(!validId(id))return "invalid_request_id";
  if(!token_[0]||std::strcmp(token_,token))return "old_session";
  const auto expiry=doc["expires_at"].as<uint64_t>();
  if(expiry<=utc)return "expired";if(expiry-utc>30)return "expiry_too_far";
  for(size_t i=0;i<count_;++i)if(!std::strcmp(ids_[i],id))return "duplicate";
  if(count_==16)return "session_exhausted";
  std::memcpy(ids_[count_++],id,std::strlen(id)+1);
  output={};std::memcpy(output.id,id,std::strlen(id)+1);output.target=std::strcmp(suffix,"bms/charge_enabled/set")?bms::MosTarget::Discharge:bms::MosTarget::Charge;
  output.state=doc["state"].as<bool>();output.deadline=now+static_cast<uint32_t>((expiry-utc)*1000);return "ok";
 }
};
}
