#include <esp_sntp.h>
#include <esp_netif.h>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <thread>
#include <cstdio>

// [MOCKED!] Raw SNTP calls require the network context. Espressif APIs marshal there.
static thread_local bool networkContext=false;
static bool enabled=true;
static unsigned configurations=0,serverWrites=0,stops=0;
static std::atomic<unsigned> concurrentConfigurations{0};
#ifdef _WIN32
extern "C" int setenv(const char* name,const char* value,int){return _putenv_s(name,value);}
#endif
extern "C" int esp_netif_init() {
    assert(concurrentConfigurations.fetch_add(1)==0 && "NTP configurations must not interleave");
    return 0;
}
extern "C" bool sntp_enabled(){assert(networkContext && "raw SNTP called outside TCP/IP task");return enabled;}
extern "C" void sntp_stop(){assert(networkContext);enabled=false;++stops;}
extern "C" void sntp_setoperatingmode(esp_sntp_operatingmode_t){assert(networkContext && !enabled);}
extern "C" void sntp_setservername(uint8_t index,char* name){assert(networkContext && !enabled && index<3);if(index<2)assert(name && std::strstr(name,".invalid"));else assert(!name);++serverWrites;}
extern "C" void sntp_init(){assert(networkContext && !enabled);enabled=true;++configurations;assert(concurrentConfigurations.fetch_sub(1)==1);}
extern "C" bool esp_sntp_enabled(){networkContext=true;bool value=sntp_enabled();networkContext=false;return value;}
extern "C" void esp_sntp_stop(){networkContext=true;sntp_stop();networkContext=false;}
extern "C" void esp_sntp_setoperatingmode(esp_sntp_operatingmode_t mode){networkContext=true;sntp_setoperatingmode(mode);networkContext=false;}
extern "C" void esp_sntp_setservername(uint8_t index,const char* name){networkContext=true;sntp_setservername(index,const_cast<char*>(name));networkContext=false;}
extern "C" void esp_sntp_init(){networkContext=true;sntp_init();networkContext=false;}

#if SBT_RAW_NTP_BASELINE
#include "raw_ntp_baseline.h"
#define TEST_CONFIG_NTP configTzTime
#else
extern "C" void sbtConfigTzTime(const char*,const char*,const char*,const char*);
#define TEST_CONFIG_NTP sbtConfigTzTime
#endif
int main(){
#if SBT_RAW_NTP_BASELINE
    TEST_CONFIG_NTP("UTC0","one.invalid","two.invalid",nullptr);
#else
    auto configure=[](){for(unsigned i=0;i<100;++i)TEST_CONFIG_NTP("UTC0","one.invalid","two.invalid",nullptr);};
    std::thread timer(configure),wifi(configure);timer.join();wifi.join();
    assert(configurations==200 && serverWrites==600 && stops==200);
    assert(enabled && concurrentConfigurations==0);
    assert(std::strcmp(std::getenv("TZ"),"UTC0")==0);
#endif
    std::puts("PASS: concurrent NTP requests retain timezone/servers and use thread-safe SDK APIs");
}
