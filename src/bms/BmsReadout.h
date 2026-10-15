#pragma once
#include "JbdProtocol.h"
#include "victron/VictronReadout.h"
namespace bms {
// All protocol and radio operations belong to the application loop. UI callbacks only copy settings.
class Readout {
 victron::ValidatedText mac_;
 victron::ConfigMailbox mailbox_;
 cm::BluetoothManager* bluetooth_=nullptr;
 victron::Configuration active_{};uint32_t seen_=0,epoch_=0,query_=0,sentAt_=0,dueAt_=0;
 bool attached_=false,connected_=false,connectRequested_=false,waiting_=false;
 uint8_t next_=3,expected_=0;
 Parser parser_;Frame frame_;Cycle cycle_;
 mutable std::mutex mutex_;Snapshot view_{};const char* reason_="configuration_missing",*viewReason_="configuration_missing";
 void reset(const char* reason){parser_.reset();cycle_.reset();connected_=false;waiting_=false;query_=0;next_=3;reason_=reason;}
 void publishView(uint32_t now){auto next=cycle_.snapshot(now);
  if(!active_.macValid||!connected_||bluetooth_->isPaused())next={};
  std::lock_guard<std::mutex> lock(mutex_);view_=next;viewReason_=next.available?"ok":bluetooth_->isPaused()?"paused":reason_;
 }
public:
 explicit Readout(const char* defaultMac):mac_(ConfigOptions<String>{.key="BmsReadMac",.name="BMS Bluetooth MAC",.category="BMS",.defaultValue=String(defaultMac),.sortOrder=1},false){}
 bool attach(ConfigManagerClass& manager,cm::BluetoothManager& bluetooth){
  if(attached_)return true;
  if(mac_.hasError()||manager.addSetting(&mac_)!=&mac_)return false;
  manager.setCategoryLayoutOverride("BMS","Bluetooth","BMS","Readout",42);
  manager.addSettingsCard("Bluetooth","BMS",42);manager.addSettingsGroup("Bluetooth","BMS","Readout",42);
  manager.addToSettingsGroup(mac_.getKey(),"Bluetooth","BMS","Readout",1);
  mac_.setCallback([this](const String& text){mailbox_.setMac(text.c_str(),text.length());});
  bluetooth_=&bluetooth;
  manager.getRuntime().addRuntimeProvider("bms",[this](JsonObject& data){
   Snapshot copy;const char* reason;{std::lock_guard<std::mutex> lock(mutex_);copy=view_;reason=viewReason_;}
   data["availability"]=copy.available?"available":"unavailable";data["reason"]=reason;
   for(size_t i=0;i<telemetry::channelCount;++i){const auto& c=telemetry::channels[i];if(c.source!=telemetry::Source::Bms)continue;
    const char* key=std::strchr(c.suffix,'/')+1;auto value=copy.values[i];
    if(!value.valid)data[key]=nullptr;else if(c.type==telemetry::FieldType::Boolean)data[key]=value.number!=0;else data[key]=value.number;
    data["types"][key]=telemetry::typeName(c.type);data["support"][key]="jbd_readout";
   }
  },7);
  attached_=true;return true;
 }
 void afterLoad(){mailbox_.setMac(mac_.get().c_str(),mac_.get().length());loop(millis());}
 void onEvent(const cm::BleEvent& event){
  if(!active_.macValid||std::strcmp(event.peer.text,active_.mac)!=0)return;
  const uint32_t now=millis();
  if(event.type==cm::BleEventType::Connected){reset("waiting_for_response");connected_=true;epoch_=event.generation;dueAt_=now;}
  else if(event.type==cm::BleEventType::Disconnected||event.type==cm::BleEventType::Error){
   reset("disconnected_or_timeout");
   // A failed GATT write may leave the transport connected without a pending query.
   // Reconfigure to cancel that link; otherwise neither layer would retry.
   if(event.type==cm::BleEventType::Error&&event.errorCode==-1003){
    cm::BleAddress peer{};std::memcpy(peer.text,active_.mac,sizeof(peer.text));
    bluetooth_->configureGattPeer(peer,0xff00,0xff01,0xff02);
    epoch_=bluetooth_->generation();connectRequested_=false;dueAt_=now+1000;
   }
  }
  else if(event.type==cm::BleEventType::Notification&&connected_&&waiting_&&event.generation==epoch_&&event.queryId&&uint32_t(now-sentAt_)<2000){
   if(query_&&query_!=event.queryId)return;query_=event.queryId;
   for(size_t i=0;i<event.size;++i)if(parser_.push(event.data[i],frame_)){
    if(frame_.reg!=expected_)continue;
    const bool accepted=cycle_.accept(frame_,now);
    // Complete during the notification callback, retaining CM's exact query identity.
    if(!bluetooth_->completeQuery()){reset("completion_failed");break;}
    waiting_=false;query_=0;parser_.reset();
    if(accepted&&expected_==3){next_=4;dueAt_=now+50;}
    else {next_=3;dueAt_=now+5000;reason_=accepted?"stale":"invalid_response";}
    break;
   }
  }
  publishView(now);
 }
 void loop(uint32_t now){
  if(!attached_)return;
  victron::Configuration config;
  if(mailbox_.take(seen_,config)){
   active_=config;reset(config.macValid?"connecting":"configuration_missing");connectRequested_=false;
   if(config.macValid){cm::BleAddress peer{};std::memcpy(peer.text,config.mac,sizeof(peer.text));peer.type=0;
    bluetooth_->configureGattPeer(peer,0xff00,0xff01,0xff02);
   }else bluetooth_->clearGattPeer();
   epoch_=bluetooth_->generation();dueAt_=now+100;
  }
  if(epoch_!=bluetooth_->generation()){
   epoch_=bluetooth_->generation();reset("reconnecting");connectRequested_=false;dueAt_=now+250;
  }
  if(active_.macValid&&!bluetooth_->isPaused()){
   if(!connected_&&!connectRequested_&&int32_t(now-dueAt_)>=0)connectRequested_=bluetooth_->requestConnect();
   if(waiting_&&uint32_t(now-sentAt_)>=2000)reset("response_timeout");
   if(connected_&&!waiting_&&int32_t(now-dueAt_)>=0){uint8_t command[7]{};
    if(readCommand(next_,command)&&bluetooth_->writeCommand(command,sizeof(command))){
     parser_.reset();expected_=next_;waiting_=true;sentAt_=now;query_=0;
    }
   }
  }
  publishView(now);
 }
 bool configured()const{return active_.macValid;}
 Snapshot snapshot()const{std::lock_guard<std::mutex> lock(mutex_);return view_;}
};
}
