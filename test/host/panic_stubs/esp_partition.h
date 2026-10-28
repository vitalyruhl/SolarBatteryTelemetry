#pragma once
#include <cstdint>
#include <cstddef>
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_COREDUMP=3;
struct esp_partition_t {uint32_t address,size;};
const esp_partition_t* esp_partition_find_first(int,int,const char*);

int esp_partition_read(const esp_partition_t*,std::size_t,void*,std::size_t);
