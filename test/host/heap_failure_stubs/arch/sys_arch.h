#pragma once
// [MOCKED!] Match the installed ESP32 lwIP semaphore ABI.
using sys_sem_t = void*;
extern "C" sys_sem_t* sys_thread_sem_get();
