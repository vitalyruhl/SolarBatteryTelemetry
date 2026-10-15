#pragma once

// Preparation version; no telemetry or control behavior is implemented yet.
#define APP_VERSION "0.1.0"
#define VERSION_DATE "2026-10-15"

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#else
#include "secret/secrets.example.h"
#endif
