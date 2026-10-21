#include <BmsReadout.h>
#include <cassert>
#include <cstdio>

// [MOCKED!] Synthetic JBD frames traverse the actual parser, Cycle and Readout.
// Settings registration/mutation uses real CM Config<T>; only radio and UI are adapters.
struct Fixture {
 ConfigManagerClass manager;
 cm::BluetoothManager ble;
 bms::Readout readout{"aa:bb:cc:dd:ee:ff"};
 explicit Fixture(uint32_t start=100) {
  hostNow=start;
  assert(readout.attach(manager,ble));
  setting<bool>("BmsControl").set(true);
  readout.afterLoad();
 }
 template<class T> Config<T>& setting(const char* key) {
  for(auto* s:manager.settings) if(String(s->getKey())==hashStringForStorage(String(key))) {
   auto* typed=dynamic_cast<Config<T>*>(s); assert(typed); return *typed;
  }
  assert(false&&"setting missing"); std::abort();
 }
 void tick(uint32_t time) { hostNow=time; readout.loop(time); }
 void event(cm::BleEventType type,int error=0) {
  cm::BleEvent e{}; e.type=type; e.generation=ble.gen; e.errorCode=error;
  std::strcpy(e.peer.text,"aa:bb:cc:dd:ee:ff"); readout.onEvent(e);
 }
 void frame(uint8_t reg,const std::vector<uint8_t>& payload,uint8_t status=0) {
  cm::BleEvent e{}; e.type=cm::BleEventType::Notification; e.generation=ble.gen; e.queryId=1;
  std::strcpy(e.peer.text,"aa:bb:cc:dd:ee:ff");
  e.size=payload.size()+7; e.data[0]=0xdd; e.data[1]=reg; e.data[2]=status; e.data[3]=uint8_t(payload.size());
  std::copy(payload.begin(),payload.end(),e.data+4);
  uint16_t sum=uint16_t(payload.size()+status); for(auto b:payload) sum+=b;
  uint16_t checksum=uint16_t(0-sum); e.data[e.size-3]=checksum>>8; e.data[e.size-2]=checksum; e.data[e.size-1]=0x77;
  readout.onEvent(e);
 }
 void basic() {
  assert(ble.lastWrite.size()==7&&ble.lastWrite[2]==3);
  std::vector<uint8_t> data(23); data[0]=0x14;data[1]=0x50;data[6]=0x27;data[7]=0x10;
  data[19]=70;data[20]=3;data[21]=14; frame(3,data);
 }
 void cells() {
  assert(ble.lastWrite.size()==7&&ble.lastWrite[2]==4);
  std::vector<uint8_t> data(28);for(unsigned i=0;i<14;++i){data[2*i]=0x0e;data[2*i+1]=0x80;}frame(4,data);
 }
 void connect() {event(cm::BleEventType::Connected);tick(hostNow);}
 void pair() {connect();basic();tick(hostNow+50);cells();assert(readout.snapshot().available);assert(readout.control().ready);}
 void unchanged(const bms::Snapshot& before) {
  const auto after=readout.snapshot();
  assert(after.available&&"complete telemetry must survive transient disconnect");
  assert(after.receivedAt==before.receivedAt&&after.sequence==before.sequence);
  for(size_t i=0;i<before.values.size();++i){assert(before.values[i].valid==after.values[i].valid);if(before.values[i].valid)assert(before.values[i].number==after.values[i].number);}
 }
 void rejectControl() {
  assert(!readout.control().ready);
  bms::MosRequest r;r.target=bms::MosTarget::Charge;r.state=false;r.deadline=hostNow+30000;std::strcpy(r.id,"host-request");
  assert(!readout.queueRequest(r)&&"cached telemetry must never authorize MOS writes");
 }
};

static void disconnectRetention(){
 Fixture f;f.pair();const auto sample=f.readout.snapshot();f.event(cm::BleEventType::Disconnected);f.unchanged(sample);f.rejectControl();
 assert(!std::strcmp(f.readout.reason(),"cached"));
 JsonDocument doc;auto object=doc.to<JsonObject>();f.manager.runtime.provider(object);assert(doc["reason"]=="cached");
 f.tick(sample.receivedAt+59999);f.unchanged(sample);f.rejectControl();
 f.tick(sample.receivedAt+60000);assert(!f.readout.snapshot().available);f.rejectControl();
}
static void reconnectQualification(){
 Fixture f;f.pair();const auto old=f.readout.snapshot();f.event(cm::BleEventType::Disconnected);
 f.connect();f.unchanged(old);f.rejectControl();
 f.basic();f.unchanged(old);f.rejectControl();
 f.tick(hostNow+50);f.cells();auto next=f.readout.snapshot();assert(next.available&&next.sequence==old.sequence+1&&next.receivedAt!=old.receivedAt);assert(f.readout.control().ready);
}
static void errors(const char* scenario){
 Fixture f;f.pair();const auto sample=f.readout.snapshot();
 if(!std::strcmp(scenario,"query_timeout")){f.tick(hostNow+5000);f.tick(hostNow+2000);}
 else if(!std::strcmp(scenario,"completion_error")){f.tick(hostNow+5000);f.ble.completionAccepted=false;f.basic();}
 else if(!std::strcmp(scenario,"invalid_response")){f.tick(hostNow+5000);f.frame(3,{},0x80);}
 else if(!std::strcmp(scenario,"generation")){++f.ble.gen;f.tick(hostNow+1);}
 else f.event(cm::BleEventType::Error,-1003);
 f.unchanged(sample);f.rejectControl();f.tick(sample.receivedAt+60000);assert(!f.readout.snapshot().available);
}
static void timeout(const char* scenario){
 const bool rollover=!std::strcmp(scenario,"rollover");Fixture f(rollover?0xfffff000u:100u);
 const int seconds=!std::strcmp(scenario,"short_timeout")?2:!std::strcmp(scenario,"long_timeout")?120:60;
 assert(f.setting<int>("BmsDataTimeout").get()==60);f.setting<int>("BmsDataTimeout").set(seconds);f.pair();auto sample=f.readout.snapshot();f.event(cm::BleEventType::Disconnected);
 f.tick(sample.receivedAt+uint32_t(seconds)*1000-1);f.unchanged(sample);f.rejectControl();
 f.tick(sample.receivedAt+uint32_t(seconds)*1000);assert(!f.readout.snapshot().available);
}
static void hardClear(const char* scenario){
 Fixture f;f.pair();
 if(!std::strcmp(scenario,"ota"))f.ble.paused=true;
 else f.setting<String>("BmsReadMac").set(!std::strcmp(scenario,"mac_change")?"11:22:33:44:55:66":"");
 f.tick(hostNow+1);assert(!f.readout.snapshot().available);f.rejectControl();
 if(f.ble.paused){f.ble.paused=false;f.tick(hostNow+1);f.connect();assert(!f.readout.snapshot().available);f.rejectControl();}
}
static void masterDisable(){Fixture f;f.pair();auto sample=f.readout.snapshot();f.setting<bool>("BmsControl").set(false);f.tick(hostNow+1);f.unchanged(sample);f.rejectControl();}
static void strictControlAge(){Fixture f;f.pair();const auto sample=f.readout.snapshot();f.tick(sample.receivedAt+29999);assert(f.readout.control().ready);f.tick(sample.receivedAt+30000);f.unchanged(sample);f.rejectControl();}
static void partialPair(){Fixture f;f.connect();f.basic();assert(!f.readout.snapshot().available);f.event(cm::BleEventType::Disconnected);f.connect();f.frame(4,std::vector<uint8_t>(28,1));assert(!f.readout.snapshot().available);f.rejectControl();}
static void strictSettings(){
 Fixture f;auto& setting=f.setting<int>("BmsDataTimeout");
 for(const char* input:{"0","-1","10000","2147483648","60.5","\"60\"","true","null"}){
  JsonDocument doc;assert(!deserializeJson(doc,input));assert(!setting.fromJSON(doc.as<JsonVariant>()));assert(setting.get()==60);
 }
 for(int valid:{1,9999}){JsonDocument doc;doc.set(valid);assert(setting.fromJSON(doc.as<JsonVariant>()));assert(setting.get()==valid);JsonDocument output;auto object=output.to<JsonObject>();f.manager.runtime.provider(object);assert(output["data_timeout_s"]==valid);}
}
static void historicalSettings(){
 Fixture f;auto& setting=f.setting<int>("BmsDataTimeout");Preferences prefs;
 for(int old:{0,10000}){prefs.putInt(setting.getKey(),old);setting.load(prefs);f.readout.afterLoad();JsonDocument doc;auto object=doc.to<JsonObject>();f.manager.runtime.provider(object);assert(doc["data_timeout_s"]==(old==0?1:9999));}
}
static void queueGate(const char* scenario){
 Fixture f;
 const bool shortAge=!std::strcmp(scenario,"queue_short");
 if(shortAge)f.setting<int>("BmsDataTimeout").set(1);
 f.pair();auto sample=f.readout.snapshot();
 // Simulate a callback between loop iterations: the cached UI-ready bit is still true.
 if(!std::strcmp(scenario,"queue_pause"))f.ble.paused=true;
 else hostNow=sample.receivedAt+(shortAge?1000u:30000u);
 assert(f.readout.control().ready);
 bms::MosRequest request;request.target=bms::MosTarget::Charge;request.deadline=hostNow+30000;std::strcpy(request.id,"callback-gate");
 assert(!f.readout.queueRequest(request));
}
static void shortControl(){Fixture f;f.setting<int>("BmsDataTimeout").set(1);f.pair();auto sample=f.readout.snapshot();f.tick(sample.receivedAt+999);assert(f.readout.control().ready);f.tick(sample.receivedAt+1000);assert(!f.readout.snapshot().available);f.rejectControl();}
int main(int argc,char** argv){
 assert(argc==2);const char* scenario=argv[1];
 if(!std::strcmp(scenario,"disconnect"))disconnectRetention();
 else if(!std::strcmp(scenario,"reconnect"))reconnectQualification();
 else if(!std::strcmp(scenario,"short_timeout")||!std::strcmp(scenario,"long_timeout")||!std::strcmp(scenario,"rollover"))timeout(scenario);
 else if(!std::strcmp(scenario,"ota")||!std::strcmp(scenario,"mac_change")||!std::strcmp(scenario,"mac_empty"))hardClear(scenario);
 else if(!std::strcmp(scenario,"master_disable"))masterDisable();
 else if(!std::strcmp(scenario,"control_age"))strictControlAge();
 else if(!std::strcmp(scenario,"partial_pair"))partialPair();
 else if(!std::strcmp(scenario,"settings"))strictSettings();
 else if(!std::strcmp(scenario,"historical_settings"))historicalSettings();
 else if(!std::strcmp(scenario,"short_control"))shortControl();
 else if(!std::strcmp(scenario,"queue_short")||!std::strcmp(scenario,"queue_pause")||!std::strcmp(scenario,"queue_stale"))queueGate(scenario);
 else {assert(!std::strcmp(scenario,"query_timeout")||!std::strcmp(scenario,"completion_error")||!std::strcmp(scenario,"invalid_response")||!std::strcmp(scenario,"generation")||!std::strcmp(scenario,"write_error"));errors(scenario);}
 std::printf("BMS Readout %s passed\n",scenario);
}
