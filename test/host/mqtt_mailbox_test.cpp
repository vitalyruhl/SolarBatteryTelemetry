#include "telemetry/MqttConfigMailbox.h"
#include <cassert>
#include <cstdio>
#include <string>
using namespace telemetry;
int main(){
 char first[65],second[65];
 assert(formatClientId(0x112233ccbbaaULL,first,sizeof(first)));
 assert(formatClientId(0x445566ccbbaaULL,second,sizeof(second)));
 assert(std::string(first)=="SolarBatteryTelemetry-112233ccbbaa"&&std::string(first)!=second);
 assert(!formatClientId(1,first,8));
 ConfigMailbox box;MqttConfig c;uint32_t seen=0;
 assert(box.take(seen,c));assert(!c.enabled);
 assert(box.text(TextField::Root,"first/base",10));box.enabled(true);
 // OTA does not consume: successive UI callbacks overwrite one owned snapshot, never enqueue history.
 assert(box.text(TextField::Root,"latest/base",11));assert(box.text(TextField::Server,"broker.example",14));
 assert(box.take(seen,c)&&std::string(c.root)=="latest/base"&&std::string(c.server)=="broker.example"&&c.enabled);
 assert(!box.take(seen,c));
 assert(!box.text(TextField::Root,"invalid/#",9));assert(box.take(seen,c)&&c.rootRejected&&std::string(c.root)=="latest/base");
 assert(!box.text(TextField::Server,std::string(129,'x').c_str(),129));assert(box.take(seen,c)&&c.configRejected&&std::string(c.server)=="broker.example");
 assert(box.text(TextField::Password,"",0));assert(box.take(seen,c)&&c.password[0]==0);
 assert(!box.port(0));assert(box.port(1884));box.intervals(1,5000);assert(box.take(seen,c)&&c.port==1884&&c.publishMs==5000&&c.listenMs==1000);
 box.intervals(10,0);assert(box.take(seen,c)&&c.publishMs==10000&&c.listenMs==0);
 assert(box.text(TextField::Root,"repaired",8));assert(box.take(seen,c)&&!c.rootRejected&&std::string(c.root)=="repaired");
 std::puts("MQTT mailbox: latest handoff, invalid preservation, empty password, bounds, interval effects passed");
}
