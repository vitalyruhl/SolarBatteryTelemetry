#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <esp_heap_caps.h>
#include "diagnostics/HeapFailureDiagnostics.h"
static unsigned allocations=0;
void* operator new(size_t size) {++allocations;if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
#if CM_ENABLE_VERBOSE_LOGGING || SBT_ENABLE_HEAP_DIAGNOSTICS
static esp_alloc_failed_hook_t registered=nullptr;
static unsigned logs=0,queries=0;
static char captured[384];
int heap_caps_register_failed_alloc_callback(esp_alloc_failed_hook_t hook) {registered=hook;return ESP_OK;}
void heap_caps_get_info(multi_heap_info_t* info,uint32_t caps) {
 ++queries; assert(caps==MALLOC_CAP_DEFAULT || caps==4);
 *info=caps==MALLOC_CAP_DEFAULT ? multi_heap_info_t{700,0,96,400,0,0,0} : multi_heap_info_t{900,0,128,600,0,0,0};
}
int ets_printf(const char* format,...) {
 ++logs;va_list args;va_start(args,format);int n=std::vsnprintf(captured,sizeof(captured),format,args);va_end(args);return n;
}
#endif
int main() {
 const unsigned before=allocations;
 SBT_INSTALL_HEAP_FAILURE_DIAGNOSTICS();
#if CM_ENABLE_VERBOSE_LOGGING || SBT_ENABLE_HEAP_DIAGNOSTICS
 assert(registered);registered(4096,4,"heap_caps_malloc");
 assert(logs==1 && queries==2);
 assert(std::strstr(captured,"size=4096 caps=0x4 api=heap_caps_malloc"));
 assert(std::strstr(captured,"free=900 largest=128 defaultFree=700 defaultLargest=96"));
 registered(8,MALLOC_CAP_DEFAULT,nullptr);
 assert(std::strstr(captured,"api=unknown"));
 for(unsigned i=0;i<40;++i) registered(1536,4,"heap_caps_malloc");
 assert(logs==16 && queries==32);
 // A fatal small C++ allocation must remain visible after large recoverable failures.
 registered(32,MALLOC_CAP_DEFAULT,"heap_caps_malloc");
 if(logs!=17 || queries!=34) {std::fputs("Late critical allocation failure was suppressed.\n",stderr);return 1;}
 assert(std::strstr(captured,"size=32"));
 for(unsigned i=0;i<21;++i) registered(1536,4,"heap_caps_malloc");
 assert(logs==18 && queries==36);
 assert(std::strstr(captured,"n=64 size=1536"));
#endif
 assert(allocations==before);
 std::puts("Heap-failure diagnostics passed: registration, capabilities, output bound, no C++ new and compile guard.");
}
