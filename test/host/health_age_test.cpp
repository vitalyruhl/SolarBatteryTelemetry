#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// [MOCKED!] UI/radio boundaries only; production loop and health formatting are compiled unchanged.
static uint32_t clockNow=100, sampleAt=101, healthAt=102;
static bool available=true;
uint32_t millis(){return clockNow;}
struct Sample {std::array<int,26> values{};bool available=true;const char* reason="ok";uint32_t receivedMs=0,receivedAt=0,sequence=7;};
struct Readout {
 void loop(uint32_t){}
 Sample snapshot(){clockNow=healthAt;Sample s;s.available=available;s.receivedMs=s.receivedAt=sampleAt;return s;}
 const char* reason(){return available?"ok":"stale";}
} victronReadout,bmsReadout;
struct Bluetooth {void loop(uint32_t){clockNow=sampleAt;}} bluetooth;
struct Wifi {void update(){}};
struct Manager {Wifi wifi;Wifi& getWiFiManager(){return wifi;}void handleClient(){}} ConfigManager;
struct Service {
 struct Health {bool initialized=false,available=false;const char* reason="unknown";uint32_t sentAt=0,receivedAt=0,sequence=0;} health_[2];
 struct Statuses {std::string payload[2];void set(unsigned i,const char* p,bool){payload[i]=p;}} statuses_;
 #include "production_health.inc"
 void updateVictron(const std::array<int,26>&,bool a,const char* r,uint32_t at,uint32_t now){health(1,a,r,at,0,now);}
 void updateBms(const Sample& s,const char* r,uint32_t now){health(0,s.available,r,s.receivedAt,s.sequence,now);}
 void loop(uint32_t){}
} telemetryMqtt;
void updateStatusLED(){}
namespace cm::helpers {struct PulseOutput {static void loopAll(){}};}
struct Logger {void loop(){}} lmg;
void delay(unsigned){}
#include "production_loop.inc"
static void run(uint32_t initial,uint32_t received,uint32_t generated,bool sourceAvailable,const char* expected){
 clockNow=initial;sampleAt=received;healthAt=generated;available=sourceAvailable;telemetryMqtt=Service{};loop();
 for(unsigned i=0;i<2;++i){const auto& value=telemetryMqtt.statuses_.payload[i];if(value.find(expected)==std::string::npos){std::fprintf(stderr,"source %u expected %s, received %s\n",i,expected,value.c_str());std::abort();}}
}
int main(){
 // New notification at T+1 must be aged only after its snapshot at T+2.
 run(100,101,102,true,"\"sample_age_ms\":1");
 run(100,100,30100,true,"\"sample_age_ms\":30000");
 run(100,100,30100,false,"\"sample_age_ms\":null");
 run(0xffffffe0u,0xfffffff0u,0x10u,true,"\"sample_age_ms\":32");
 std::puts("Production loop health ordering, both sources, unavailable/expiry and uint32 rollover passed");
}
