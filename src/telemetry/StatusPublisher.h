#pragma once
#include "TelemetryPublisher.h"
namespace telemetry {
inline bool formatCommandResult(const char* id,const char* phase,const char* reason,const Value* values,bool available,char* out,size_t capacity){
 char charge[8]="null",discharge[8]="null",current[32]="null";
 if(available&&values){formatValue(channels[8],values[8],charge,sizeof(charge));formatValue(channels[9],values[9],discharge,sizeof(discharge));formatValue(channels[4],values[4],current,sizeof(current));}
 const int n=std::snprintf(out,capacity,"{\"request_id\":\"%s\",\"phase\":\"%s\",\"reason\":\"%s\",\"charge_enabled\":%s,\"discharge_enabled\":%s,\"observed_current_a\":%s}",id,phase,reason,charge,discharge,current);
 return n>=0&&static_cast<size_t>(n)<capacity;
}

class StatusPublisher {
 struct Slot {bool valid=false,dirty=false;};Slot slots_[4]{};
 // Match the existing health/control producers; each slot keeps independent retry ownership.
 char payloads_[256+256+384+384]{};
 static constexpr size_t capacity(unsigned index){return index<2?256:384;}
 char* payload(unsigned index){return payloads_+(index<2?index*256:512+(index-2)*384);}
 uint32_t retryAt_=0;bool retry_=false;unsigned cursor_=0;
public:
 bool set(unsigned index,const char* payload,bool force=false){
  if(index>=4||!payload||std::strlen(payload)>=capacity(index))return false;
  auto& slot=slots_[index];char* stored=this->payload(index);if(force||!slot.valid||std::strcmp(stored,payload)){std::memcpy(stored,payload,std::strlen(payload)+1);slot.valid=slot.dirty=true;}return true;
 }
 void clearResult(){slots_[3]={};std::memset(payload(3),0,capacity(3));}
 void onConnected(){clearResult();for(auto& slot:slots_)slot.dirty=slot.valid;retry_=false;}
 void tick(uint32_t now,const char* root,Transport& wire){
  if(!wire.connected()||(retry_&&uint32_t(now-retryAt_)<250))return;retry_=false;
  constexpr const char* suffixes[]={"bms/health","victron/health","bms/control","bms/control/result"};
  for(unsigned i=0;i<4;++i){const auto index=(cursor_+i)%4;auto& slot=slots_[index];if(!slot.dirty)continue;
   char topic[topicLimit+1];if(!buildTopic(root,suffixes[index],topic,sizeof(topic)))return;
   if(wire.publish(topic,payload(index),index!=3)){slot.dirty=false;cursor_=(index+1)%4;}
   else {retry_=true;retryAt_=now;}return;
  }
 }
};
}
