#pragma once
#include "PanicSummary.h"
#include "telemetry/TelemetryPublisher.h"

namespace sbt::diagnostics {
// Capture is boot-owned; tick is called only by the existing MQTT wire owner.
// A successful QoS0 write is local acceptance, not a broker acknowledgement.
class PanicSummaryPublisher {
 char payload_[telemetry::payloadLimit+1]{};
 bool captured_=false,ready_=false,sent_=false;
 uint8_t attempts_=0;uint32_t attemptedAt_=0;
public:
 bool capture(const PanicSummary& summary){
  if(captured_)return false;
  captured_=true;ready_=formatPanicSummary(summary,payload_,sizeof(payload_));return ready_;
 }
 void tick(uint32_t now,const char* root,telemetry::Transport& wire){
  if(!ready_||sent_||attempts_>=12||!wire.connected())return;
  if(attempts_&&uint32_t(now-attemptedAt_)<5000)return;
  char topic[telemetry::topicLimit+1];
  if(!telemetry::buildTopic(root,"Diagnostics/Panic",topic,sizeof(topic)))return;
  attemptedAt_=now;++attempts_;sent_=wire.publish(topic,payload_,false);
 }
};
}
