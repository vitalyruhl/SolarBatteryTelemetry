#pragma once
// [MOCKED!] ESP-IDF network initialization facade.
extern "C" int esp_netif_init();
extern "C" int setenv(const char*,const char*,int);
