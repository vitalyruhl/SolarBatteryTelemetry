#pragma once
// [MOCKED!] Allocator registration and snapshot APIs only.
#include <cstddef>
#include <cstdint>
constexpr uint32_t MALLOC_CAP_DEFAULT=1u<<12;
constexpr int ESP_OK=0;
struct multi_heap_info_t { size_t total_free_bytes,total_allocated_bytes,largest_free_block,
    minimum_free_bytes,allocated_blocks,free_blocks,total_blocks; };
using esp_alloc_failed_hook_t=void(*)(size_t,uint32_t,const char*);
int heap_caps_register_failed_alloc_callback(esp_alloc_failed_hook_t);
void heap_caps_get_info(multi_heap_info_t*,uint32_t);
