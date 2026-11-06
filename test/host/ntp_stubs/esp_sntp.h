#pragma once
#include <cstdint>
// [MOCKED!] SDK facade checks the raw-versus-thread-safe API boundary.
enum esp_sntp_operatingmode_t { SNTP_OPMODE_POLL = 0 };
extern "C" {
bool esp_sntp_enabled();
void esp_sntp_stop();
void esp_sntp_setoperatingmode(esp_sntp_operatingmode_t);
void esp_sntp_setservername(uint8_t, const char*);
void esp_sntp_init();
bool sntp_enabled();
void sntp_stop();
void sntp_setoperatingmode(esp_sntp_operatingmode_t);
void sntp_setservername(uint8_t, char*);
void sntp_init();
}
