#pragma once
#include "BmsCommandPolicy.h"
#include "runtime/AppRuntimeMetadata.h"
#include <atomic>
#include "victron/VictronReadout.h"
namespace bms {
// All protocol and radio operations belong to the application loop. UI callbacks only copy settings.
class Readout {
 Config<bool> controlEnabled_{ConfigOptions<bool>{.key="BmsControl",.name="BMS control enabled",.category="BMS",.defaultValue=false,.sortOrder=2}};
 std::atomic<bool> enabled_{false},cancelRequested_{false};
 BmsCommandPolicy commands_;MosMailbox requests_;bool controlQuery_=false;
 struct ControlView {bool enabled=false,ready=false,pending=false;MosResult result{};const char* admission="none";};
 ControlView controlView_{};uint32_t uiRequest_=0;
 victron::ValidatedText mac_;
 victron::ConfigMailbox mailbox_;
 cm::BluetoothManager* bluetooth_=nullptr;
 victron::Configuration active_{};uint32_t seen_=0,epoch_=0,query_=0,sentAt_=0,dueAt_=0;
 bool attached_=false,connected_=false,connectRequested_=false,waiting_=false;
 uint8_t next_=3,expected_=0;
 Parser parser_;Frame frame_;Cycle cycle_;
 mutable std::mutex mutex_;Snapshot view_{};const char* reason_="configuration_missing",*viewReason_="configuration_missing";
 void reset(const char* reason){commands_.cancel(reason);{std::lock_guard<std::mutex> lock(mutex_);requests_.cancel();}parser_.reset();cycle_.reset();connected_=false;waiting_=false;query_=0;next_=3;reason_=reason;}
 void publishView(uint32_t now){auto next=cycle_.snapshot(now);
  if(!active_.macValid||!connected_||bluetooth_->isPaused())next={};
  std::lock_guard<std::mutex> lock(mutex_);view_=next;viewReason_=next.available?"ok":bluetooth_->isPaused()?"paused":reason_;
  requests_.settle(commands_.pending());
  controlView_.enabled=enabled_.load();controlView_.pending=requests_.pending()||commands_.pending();controlView_.admission=requests_.reason();
  controlView_.ready=enabled_.load()&&next.available&&!bluetooth_->isPaused()&&!controlView_.pending;
  controlView_.result=commands_.result();
 }
public:
 explicit Readout(const char* defaultMac):mac_(ConfigOptions<String>{.key="BmsReadMac",.name="BMS Bluetooth MAC",.category="BMS",.defaultValue=String(defaultMac),.sortOrder=1},false){}
 bool attach(ConfigManagerClass& manager,cm::BluetoothManager& bluetooth){
  if(attached_)return true;
  if(mac_.hasError()||manager.addSetting(&mac_)!=&mac_||controlEnabled_.hasError()||manager.addSetting(&controlEnabled_)!=&controlEnabled_)return false;
  manager.setCategoryLayoutOverride("BMS","Bluetooth","BMS","Readout",42);
  manager.addSettingsCard("Bluetooth","BMS",42);manager.addSettingsGroup("Bluetooth","BMS","Readout",42);
  manager.addToSettingsGroup(mac_.getKey(),"Bluetooth","BMS","Readout",1);
  manager.addToSettingsGroup(controlEnabled_.getKey(),"Bluetooth","BMS","Readout",2);
  controlEnabled_.setCallback([this](bool value){enabled_.store(value);if(!value)cancelRequested_.store(true);});
  mac_.setCallback([this](const String& text){mailbox_.setMac(text.c_str(),text.length());});
  bluetooth_=&bluetooth;
  manager.getRuntime().addRuntimeProvider("bms",[this](JsonObject& data){
   Snapshot copy;const char* reason;{std::lock_guard<std::mutex> lock(mutex_);copy=view_;reason=viewReason_;}
   data["availability"]=copy.available?"available":"unavailable";data["reason"]=reason;
   data["sample_sequence"]=copy.sequence;
   if(copy.available)data["sample_age_ms"]=uint32_t(millis()-copy.receivedAt);else data["sample_age_ms"]=nullptr;
   auto c=control();data["control_enabled"]=c.enabled;data["control_ready"]=c.ready;data["control_pending"]=c.pending;
   data["control_admission"]=c.admission;data["control_result"]=c.result.phase;data["control_reason"]=c.result.reason;data["control_request_id"]=c.result.id;
   for(size_t i=0;i<telemetry::channelCount;++i){const auto& c=telemetry::channels[i];if(c.source!=telemetry::Source::Bms)continue;
    const char* key=std::strchr(c.suffix,'/')+1;auto value=copy.values[i];
    if(!value.valid)data[key]=nullptr;else if(c.type==telemetry::FieldType::Boolean)data[key]=value.number!=0;else if(c.type==telemetry::FieldType::Protection){char text[512];if(telemetry::formatProtection(static_cast<uint16_t>(value.number),text,sizeof(text)))data[key]=text;else data[key]=nullptr;}else data[key]=value.number;
    data["types"][key]=telemetry::typeName(c.type);data["support"][key]="jbd_readout";
   }
  },7);
  for(unsigned i=0;i<4;++i){
   if(!sbt::metadata::registerBmsButton(manager.getRuntime(),i))return false;
   manager.getRuntime().registerRuntimeButton("bms",sbt::metadata::bmsButtonKeys[i],[this,i](){MosRequest request;request.target=i<2?MosTarget::Charge:MosTarget::Discharge;request.state=(i%2)==0;request.deadline=millis()+30000;
    {std::lock_guard<std::mutex> lock(mutex_);std::snprintf(request.id,sizeof(request.id),"ui-%lu",static_cast<unsigned long>(++uiRequest_));}
    queueRequest(request);
   });
  }
  attached_=true;return true;
 }
 void afterLoad(){enabled_.store(controlEnabled_.get());mailbox_.setMac(mac_.get().c_str(),mac_.get().length());loop(millis());}
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
    const bool accepted=frame_.reg==0xe1?frame_.status==0:cycle_.accept(frame_,now);
    if(controlQuery_)commands_.response(frame_,accepted,cycle_.snapshot(now),now);
    // Complete during the notification callback, retaining CM's exact query identity.
    if(!bluetooth_->completeQuery()){reset("completion_failed");break;}
    waiting_=false;query_=0;parser_.reset();
    if(controlQuery_){next_=3;dueAt_=now+50;controlQuery_=false;}
    else if(accepted&&expected_==3){next_=4;dueAt_=now+50;}
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
  if(cancelRequested_.exchange(false)||bluetooth_->isPaused()){
   commands_.cancel(bluetooth_->isPaused()?"ota_or_paused":"cancelled_or_disabled");
   std::lock_guard<std::mutex> lock(mutex_);requests_.cancel();
  }
  if(!enabled_.load())commands_.cancel("disabled");
  commands_.tick(now);
  if(waiting_&&controlQuery_&&!commands_.pending()&&!bluetooth_->isPaused()){
   // Fence an outstanding command on cancellation; never retry an uncertain write.
   cm::BleAddress peer{};std::memcpy(peer.text,active_.mac,sizeof(peer.text));bluetooth_->configureGattPeer(peer,0xff00,0xff01,0xff02);
   reset("control_cancelled");epoch_=bluetooth_->generation();connectRequested_=false;dueAt_=now+250;
  }
  if(!waiting_&&!commands_.pending()){
   MosRequest request;bool have=false;{std::lock_guard<std::mutex> lock(mutex_);have=requests_.take(request);}
   if(have){commands_.submit(request,now,enabled_.load(),connected_&&!bluetooth_->isPaused()&&cycle_.snapshot(now).available);dueAt_=now;}
  }
  if(active_.macValid&&!bluetooth_->isPaused()){
   if(!connected_&&!connectRequested_&&int32_t(now-dueAt_)>=0)connectRequested_=bluetooth_->requestConnect();
   if(waiting_&&uint32_t(now-sentAt_)>=2000)reset("response_timeout");
   if(connected_&&!waiting_&&int32_t(now-dueAt_)>=0){uint8_t command[9]{};size_t size=0;
    const bool control=commands_.pending();bool prepared=false;
    if(control)prepared=commands_.command(command,size);
    else {uint8_t read[7];prepared=readCommand(next_,read);std::memcpy(command,read,7);size=7;}
    if(prepared&&bluetooth_->writeCommand(command,size)){
     parser_.reset();expected_=control?commands_.expected():next_;controlQuery_=control;waiting_=true;sentAt_=now;query_=0;
    }else if(control){commands_.cancel("write_admission_failed");dueAt_=now+100;}
   }
  }
  publishView(now);
 }
 bool queueRequest(const MosRequest& request){
  std::lock_guard<std::mutex> lock(mutex_);
  const auto now=millis();const bool fresh=view_.available&&uint32_t(now-view_.receivedAt)<30000;
  const auto result=requests_.offer(request,now,enabled_.load(),fresh&&controlView_.ready,cancelRequested_.load());
  controlView_.admission=result;controlView_.pending=requests_.pending();
  return !std::strcmp(result,"accepted");
 }
 void cancelCommands(){cancelRequested_.store(true);}
 ControlView control()const{std::lock_guard<std::mutex> lock(mutex_);return controlView_;}
 const char* reason()const{std::lock_guard<std::mutex> lock(mutex_);return viewReason_;}
 bool configured()const{return active_.macValid;}
 Snapshot snapshot()const{std::lock_guard<std::mutex> lock(mutex_);return view_;}
};
}
