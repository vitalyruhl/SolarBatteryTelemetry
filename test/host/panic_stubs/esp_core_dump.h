#pragma once
// [MOCKED!] Only the SDK functions used by the read-only capture adapter exist.
#include <cstddef>
#include <cstdint>
#define CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH 1
#define CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF 1
constexpr int ESP_OK=0,ESP_ERR_NOT_SUPPORTED=0x106;
struct esp_core_dump_summary_t {
 char exc_task[16];uint32_t exc_pc;
 struct {uint32_t bt[16];uint32_t depth;bool corrupted;} exc_bt_info;
 uint8_t app_elf_sha256[17];struct {uint32_t exc_cause;uint32_t exc_a[16];} ex_info;
};
int esp_core_dump_image_get(std::size_t*,std::size_t*);
int esp_core_dump_image_check();
int esp_core_dump_get_summary(esp_core_dump_summary_t*);
