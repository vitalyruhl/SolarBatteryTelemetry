#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <arch/sys_arch.h>
#include <freertos/task.h>
#include "diagnostics/ThreadSemaphoreDiagnostics.h"
extern "C" sys_sem_t* __wrap_sys_thread_sem_get();
extern bool diagnosticTestIsr;
extern TaskHandle_t diagnosticTestTask;
extern char diagnosticTestTaskName[32];
extern unsigned diagnosticTestTaskReads,diagnosticTestRealCalls;
extern sys_sem_t* diagnosticTestRealResult;
static unsigned allocations=0,logs=0;
static char captured[8192];
static size_t used=0;
void* operator new(size_t size) {++allocations;if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
int ets_printf(const char* format,...) {
 ++logs;va_list args;va_start(args,format);
 const int n=std::vsnprintf(captured+used,sizeof(captured)-used,format,args);va_end(args);
 if(n>0)used+=static_cast<size_t>(n)<sizeof(captured)-used?static_cast<size_t>(n):sizeof(captured)-used-1;
 return n;
}
int main() {
 const unsigned before=allocations;
 sys_sem_t semaphore=reinterpret_cast<void*>(0x5678);
 diagnosticTestRealResult=&semaphore;
 assert(__wrap_sys_thread_sem_get()==&semaphore);
 assert(diagnosticTestRealCalls==1 && logs==0 && diagnosticTestTaskReads==0);
 // A real getter that returns NULL must remain NULL, with exactly one call.
 sbt::diagnostics::recordAllocationFailure(80,4,"heap_caps_malloc",reinterpret_cast<void*>(0xabcd));
 diagnosticTestRealResult=nullptr;
 assert(__wrap_sys_thread_sem_get()==nullptr);
 assert(diagnosticTestRealCalls==2);
 assert(std::strstr(captured,"[SEM-NULL]"));
 assert(std::strstr(captured,"name=host-task"));
 assert(std::strstr(captured,"size=80 caps=0x4"));
 assert(std::strstr(captured,"sameTask=1"));
 assert(std::strstr(captured,"caller="));
 // Capture the name at failure time, rather than retaining a task-owned pointer.
 std::memcpy(diagnosticTestTaskName,"renamed-task",13);
 diagnosticTestTask=reinterpret_cast<void*>(0x9999);
 used=0;captured[0]=0;
 assert(__wrap_sys_thread_sem_get()==nullptr && diagnosticTestRealCalls==3);
 assert(std::strstr(captured,"name=renamed-task"));
 assert(std::strstr(captured,"name=host-task"));
 assert(std::strstr(captured,"sameTask=0"));
 // The failure hook may run in ISR context: task identity APIs are not safe there.
 diagnosticTestIsr=true;
 const auto taskReads=diagnosticTestTaskReads;
 sbt::diagnostics::recordAllocationFailure(48,4,"heap_caps_malloc",nullptr);
 assert(diagnosticTestTaskReads==taskReads);
 // Ring eviction preserves the four most recent failures, including an 80-byte semaphore block.
 diagnosticTestIsr=false;
 for(unsigned i=0;i<5;++i) sbt::diagnostics::recordAllocationFailure(76+i,4,"heap_caps_malloc",nullptr);
 used=0;captured[0]=0;
 assert(__wrap_sys_thread_sem_get()==nullptr && diagnosticTestRealCalls==4);
 assert(!std::strstr(captured,"size=76 "));
 assert(std::strstr(captured,"size=77 ") && std::strstr(captured,"size=80 "));
 assert(allocations==before);
 std::puts("Thread semaphore diagnostics passed: single getter call, NULL correlation, task snapshot, ISR guard and no C++ new.");
}
