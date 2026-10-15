#pragma once

// WiFi/UI/OTA and optional BLE-worker smoke runtime; no telemetry/control.
#define APP_VERSION "0.2.0"
#define VERSION_DATE "2026-10-15"

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define SMOKE_LOCAL_SECRETS 1
#else
#include "secret/secrets.example.h"
#define SMOKE_LOCAL_SECRETS 0
#endif
