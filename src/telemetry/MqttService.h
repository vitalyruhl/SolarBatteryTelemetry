#pragma once
#include <Arduino.h>
#include <ConfigManager.h>
#include <mqtt/MQTTManager.h>
#include <atomic>
#include "MqttConfigMailbox.h"
#include "MqttLoopPolicy.h"
#include "MqttUiSettings.h"
namespace telemetry {
class MqttService final : public Transport {
 // UI/persistence and network settings have separate objects and key namespaces.
 std::unique_ptr<MqttUiSettings> ui_;
 cm::MQTTManager& mqtt_=cm::MQTTManager::instance();
 ConfigMailbox mailbox_;
 MqttConfig desired_{};uint32_t seen_=0;
 Publisher publisher_;
 bool oldEndpointUnconfirmed_=false;
 std::atomic<bool> otaPaused_{false};
 struct View {char root[rootLimit+1]="SolarBatteryTelemetry";bool enabled=false,connected=false,rejected=false,delivered=false,cleanupPending=false,oldEndpointUnconfirmed=false;int state=0;};
 std::mutex viewMutex_;View view_;
 void captureText(TextField field,const String& value){mailbox_.text(field,value.c_str(),value.length());}
 void captureInitial() {
  mailbox_.enabled(ui_->enableMQTT.get());captureText(TextField::Server,ui_->server.get());mailbox_.port(ui_->port.get());
  captureText(TextField::Username,ui_->username.get());captureText(TextField::Password,ui_->password.get());
  captureText(TextField::ClientId,ui_->clientId.get());captureText(TextField::Root,ui_->publishTopicBase.get());
  mailbox_.intervals(ui_->publishIntervalSec.get(),ui_->listenIntervalMs.get());
 }
 void installCallbacks(){
  ui_->enableMQTT.setCallback([this](bool v){mailbox_.enabled(v);});
  ui_->server.setCallback([this](const String& v){captureText(TextField::Server,v);});
  ui_->port.setCallback([this](int v){mailbox_.port(v);});
  ui_->username.setCallback([this](const String& v){captureText(TextField::Username,v);});
  ui_->password.setCallback([this](const String& v){captureText(TextField::Password,v);});
  ui_->clientId.setCallback([this](const String& v){captureText(TextField::ClientId,v);});
  ui_->publishTopicBase.setCallback([this](const String& v){captureText(TextField::Root,v);});
  ui_->publishIntervalSec.setCallback([this](float v){mailbox_.publishInterval(v);});
  ui_->listenIntervalMs.setCallback([this](int v){mailbox_.listenInterval(v);});
 }
 void updateView(){std::lock_guard<std::mutex> guard(viewMutex_);std::memcpy(view_.root,publisher_.root(),std::strlen(publisher_.root())+1);
  view_.enabled=desired_.enabled;view_.connected=mqtt_.isConnected();view_.rejected=desired_.rootRejected||desired_.configRejected||publisher_.rootRejected();
  view_.cleanupPending=publisher_.retirementPending();view_.oldEndpointUnconfirmed=oldEndpointUnconfirmed_;
  view_.delivered=publisher_.delivered();view_.state=static_cast<int>(mqtt_.getState());}
 static void field(ConfigManagerClass& cm,const char* source,const char* key,const char* label,int order,const char* card="MQTT",const char* unit=""){
  RuntimeFieldMeta meta;meta.sourceGroup=source;meta.key=key;meta.label=label;meta.page=std::strcmp(source,"bms")==0?"BMS":std::strcmp(source,"victron")==0?"Victron":"MQTT";meta.card=card;meta.group=card;
  meta.order=order;meta.isString=true;meta.unit=unit;cm.getRuntime().addRuntimeMeta(meta);
 }
public:
 bool attach(ConfigManagerClass& cm,const char* broker,int port,const char* user,const char* password,bool enableDefault){
  if(ui_)return false; // Persistent settings must be constructed and registered only once.
  char client[65];formatClientId(ESP.getEfuseMac(),client,sizeof(client));
  ui_=std::make_unique<MqttUiSettings>(broker,port,user,password,enableDefault,client);
  Preferences prefs;
  if(!prefs.begin("ConfigManager",false))return false;
  const bool migrated=ui_->migrate(prefs);
  prefs.end();
  if(!migrated)return false;
  cm.setCategoryLayoutOverride("MQTT","MQTT","MQTT","MQTT Settings",40);cm.addSettingsPage("MQTT",40);cm.addSettingsGroup("MQTT","MQTT","MQTT Settings",40);
  if(!ui_->attachTo(cm))return false;
  // Deliberately no MQTTManager::attach(): it would expose loop-owned settings to AsyncWeb writes.
  // Standalone public settings/begin/loop retain System-Info/ESP and /WiFi conventions.
  mqtt_.setBufferSize(1536);mqtt_.setRetryInterval(10000);mqtt_.setMaxRetries(10);mqtt_.setKeepAlive(30);
  mqtt_.onMQTTConnect([this](){publisher_.onConnected();});
  installCallbacks();
  cm.addLivePage("MQTT",45);cm.addLivePage("BMS",50);cm.addLivePage("Victron",55);
  for(const char* card:{"MQTT","bms","victron"}){
   const char* page=std::strcmp(card,"bms")==0?"BMS":std::strcmp(card,"victron")==0?"Victron":"MQTT";
   cm.addLiveCard(page,card,45);cm.addLiveGroup(page,card,card,45);
  }
  cm.getRuntime().addRuntimeProvider("telemetry",[this](JsonObject& data){
   View v;{std::lock_guard<std::mutex> guard(viewMutex_);v=view_;}
   data["scope"]="Victron Instant Readout and JBD BMS; read-only";data["root"]=String(v.root);
   data["connection"]=v.connected?"online":"offline";data["enabled"]=v.enabled?"enabled":"disabled";
   data["rootValidation"]=v.rejected?"rejected; last valid configuration retained":"valid";
   data["snapshot"]=v.delivered?"locally sent (QoS0)":"pending";
   data["ota"]=otaPaused_.load()?"paused":"ready";
   data["retirement"]=v.oldEndpointUnconfirmed?"previous broker cleanup unconfirmed":v.cleanupPending?"same broker cleanup pending":"complete";
  },6);
  field(cm,"telemetry","scope","Preparation scope",1);field(cm,"telemetry","root","Active base topic",2);
  field(cm,"telemetry","connection","MQTT connection",3);field(cm,"telemetry","enabled","MQTT enabled",4);
  field(cm,"telemetry","rootValidation","Configuration validation",5);field(cm,"telemetry","snapshot","Latest snapshot",6);field(cm,"telemetry","ota","MQTT OTA pause",7);field(cm,"telemetry","retirement","Retained status cleanup",8);
  for(Source source:{Source::Bms,Source::Victron}) {
   const char* name=sourceName(source);
   field(cm,name,"availability","Source availability",1,name);field(cm,name,"reason","Source reason",2,name);
  }
  int order=10;for(const auto& c:channels)field(cm,sourceName(c.source),std::strchr(c.suffix,'/')+1,c.label,order++,sourceName(c.source),c.unit);
  return true;
 }
 // Called once after loadAll and before server/tasks. Config::load intentionally omits callbacks.
 void afterLoad(){captureInitial();}
 void pauseForOta(bool value){otaPaused_.store(value);}
 void updateBms(const Value* values,bool available){
  for(std::size_t i=0;i<channelCount;++i)if(channels[i].source==Source::Bms)publisher_.setValue(i,available?values[i]:Value{});
  publisher_.setSourceAvailable(Source::Bms,available);
 }
 void updateVictron(const Value* values,bool available){
  for(std::size_t i=0;i<channelCount;++i)if(channels[i].source==Source::Victron)publisher_.setValue(i,available?values[i]:Value{});
  publisher_.setSourceAvailable(Source::Victron,available);
 }
 void loop(uint32_t now){
  if(otaPaused_.load()){publisher_.setPaused(true);return;}
  publisher_.setPaused(false);
  MqttConfig next;
  if(mailbox_.take(seen_,next)){
   const bool endpointChanged=desired_.port!=next.port||std::strcmp(desired_.server,next.server);
   const bool reconnect=desired_.enabled!=next.enabled||desired_.port!=next.port||std::strcmp(desired_.server,next.server)||
    std::strcmp(desired_.username,next.username)||std::strcmp(desired_.password,next.password)||std::strcmp(desired_.clientId,next.clientId);
   desired_=next;
   if(reconnect){
    const auto cleanup=disconnectForReconfigure(publisher_,*this,endpointChanged);
    if(cleanup==CleanupResult::UnconfirmedOldEndpoint)oldEndpointUnconfirmed_=true;
    // DeferredSameEndpoint remains in Publisher; disable never waits indefinitely for the broker.
   }
   auto& live=mqtt_.settings();
   live.enableMQTT.set(next.enabled);mqtt_.setServer(next.server,next.port);mqtt_.setCredentials(next.username,next.password);mqtt_.setClientId(next.clientId);
   live.publishIntervalSec.set(next.publishMs/1000.0f);live.listenIntervalMs.set(next.listenMs);publisher_.setInterval(next.publishMs);
   if(reconnect&&next.enabled)configure(publisher_.root());
  }
  publisher_.requestRoot(desired_.root,std::strlen(desired_.root));
  // Resolve root transitions before CM's automatic System-Info publication.
  runPreparedMqttLoop(publisher_,now,*this,desired_.enabled&&desired_.server[0],
   [this](){return otaPaused_.load();},[this](){mqtt_.loop();});
  updateView();
 }
 bool connected() const override{return mqtt_.isConnected();}
 bool publish(const char* topic,const char* payload,bool retained) override {
  return !otaPaused_.load()&&std::strlen(topic)<=topicLimit&&std::strlen(payload)<=payloadLimit&&mqtt_.publishRaw(topic,payload,retained);
 }
 void disconnect() override{mqtt_.disconnect();}
 void configure(const char* root) override{
  char will[topicLimit+1];if(!buildTopic(root,"status",will,sizeof(will)))return;
  mqtt_.settings().publishTopicBase.set(String(root));mqtt_.setLastWill(will,"offline",true,0);
  if(desired_.enabled&&desired_.server[0])mqtt_.begin();
 }
};
}
