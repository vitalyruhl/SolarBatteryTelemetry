#include <Arduino.h>
#include <ConfigManager.h>
#include "settings.h"

// Compile/link preparation only. Do not start WiFi, MQTT, BLE, OTA, or GPIO.
// Runtime configuration and telemetry require the next approved work package.
void setup()
{
    Serial.begin(115200);
    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(APP_VERSION);
    Serial.println("[I] Preparation only; telemetry inactive");
}

void loop()
{
    delay(1000);
}
