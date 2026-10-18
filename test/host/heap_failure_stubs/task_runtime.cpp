// [MOCKED!] Fixed task/ISR/getter responses for diagnostic boundary tests.
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <arch/sys_arch.h>
bool diagnosticTestIsr=false;
TaskHandle_t diagnosticTestTask=reinterpret_cast<void*>(0x1234);
char diagnosticTestTaskName[32]="host-task";
unsigned diagnosticTestTaskReads=0;
unsigned diagnosticTestRealCalls=0;
sys_sem_t* diagnosticTestRealResult=nullptr;
int xPortInIsrContext() {return diagnosticTestIsr;}
TaskHandle_t xTaskGetCurrentTaskHandle() {++diagnosticTestTaskReads;return diagnosticTestTask;}
char* pcTaskGetName(TaskHandle_t) {++diagnosticTestTaskReads;return diagnosticTestTaskName;}
extern "C" sys_sem_t* __real_sys_thread_sem_get() {++diagnosticTestRealCalls;return diagnosticTestRealResult;}
