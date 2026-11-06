#include <esp_netif.h>
#include <esp_sntp.h>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include "SafeNtp.h"

// Arduino 2.0.17 configTzTime uses raw lwIP SNTP calls from the caller's task.
// CM invokes it from WiFi lifecycle and ESP_TIMER_TASK. The SDK's esp_sntp API
// marshals PCB mutations to TCP/IP; serialize whole reconfigurations as well.
extern "C" void sbtConfigTzTime(const char* timezone,const char* server1,
                               const char* server2,const char* server3) {
    static std::mutex transaction;
    std::lock_guard<std::mutex> lock(transaction);
    esp_netif_init();
    if(esp_sntp_enabled())esp_sntp_stop();
    esp_sntp_setoperatingmode(static_cast<esp_sntp_operatingmode_t>(SNTP_OPMODE_POLL));
    esp_sntp_setservername(0,server1);
    esp_sntp_setservername(1,server2);
    esp_sntp_setservername(2,server3);
    esp_sntp_init();
    setenv("TZ",timezone,1);
    tzset();
}
