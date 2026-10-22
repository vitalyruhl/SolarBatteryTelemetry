#pragma once
// [MOCKED!] Current-task identity only; no scheduler or task allocation.
using TaskHandle_t = void*;
TaskHandle_t xTaskGetCurrentTaskHandle();
char* pcTaskGetName(TaskHandle_t);
