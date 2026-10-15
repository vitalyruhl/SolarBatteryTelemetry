#include <Arduino.h>
#include <ConfigManager.h>
#include <bluetooth/BluetoothManager.h>
#include <core/CoreSettings.h>
#include <core/CoreWiFiServices.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <WiFi.h>
#include "settings.h"
#include "smoke/SmokePolicy.h"
#include "telemetry/MqttService.h"
#include "victron/VictronReadout.h"

static cm::BluetoothManager bluetooth;
static telemetry::MqttService telemetryMqtt;
static victron::Readout victronReadout(VICTRON_READOUT_MAC,VICTRON_READOUT_KEY);
static auto& core=cm::CoreSettings::instance();
static auto& wifiSettings=core.wifi;
static auto& systemSettings=core.system;
static cm::CoreWiFiServices wifiServices;
static bool appReady=false;
static bool bleWorkerReady=false; // Written before web-server startup, then immutable.

// Keep recovery diagnostics independent of HTTP/JSON and omit all credentials.
static void printHealth(const char* stage) {
    Serial.printf("[D] %s ms=%lu heap=%u min=%u largest=%u wifi=%d rssi=%d\n",
        stage, static_cast<unsigned long>(millis()), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
        static_cast<int>(WiFi.status()), WiFi.isConnected() ? WiFi.RSSI() : 0);
}

static bool attachSystemSettings() {
    // Reuse CM's original object. Provision only a genuinely missing NVS value.
    Preferences prefs;
    if (!prefs.begin("ConfigManager", false)) return false;
    const bool initialized=sbt::initializeSettingIfMissing(prefs, systemSettings.otaPassword, String(OTA_PASSWORD));
    prefs.end();
    if (!initialized) return false;
    ConfigManager.setCategoryLayoutOverride(cm::CoreCategories::System,cm::CoreCategories::System,
        cm::CoreCategories::System,"System Settings",20);
    ConfigManager.addSettingsPage(cm::CoreCategories::System,20);
    ConfigManager.addSettingsGroup(cm::CoreCategories::System,cm::CoreCategories::System,"System Settings",20);
    return ConfigManager.addSetting(&systemSettings.allowOTA) &&
        ConfigManager.addSetting(&systemSettings.otaPassword);
}

static void setupNetworkDefaults() {
#if SMOKE_LOCAL_SECRETS
    if(smoke::applyNetworkDefaults(true,wifiSettings.wifiSsid.get().length(),wifiSettings.wifiPassword.get().length(),
                                  String(MY_WIFI_SSID).length(),String(MY_WIFI_PASSWORD).length())) {
        wifiSettings.wifiSsid.set(MY_WIFI_SSID);
        wifiSettings.wifiPassword.set(MY_WIFI_PASSWORD);
        wifiSettings.staticIp.set(MY_WIFI_IP);
        wifiSettings.useDhcp.set(MY_USE_DHCP);
        wifiSettings.gateway.set(MY_GATEWAY_IP);
        wifiSettings.subnet.set(MY_SUBNET_MASK);
        wifiSettings.dnsPrimary.set(MY_DNS_IP);
        ConfigManager.saveAll();
        Serial.println("[I] Missing WiFi defaults provisioned; no forced reboot");
    }
#endif
}

static void addRuntimeField(const char* source,const char* key,const char* label,int order,bool isBool=false,bool isString=false) {
    RuntimeFieldMeta meta;
    meta.sourceGroup=source;meta.group="Smoke test";meta.page="Bluetooth";meta.card="Transport";
    meta.key=key;meta.label=label;meta.order=order;meta.isBool=isBool;meta.isString=isString;meta.precision=0;
    ConfigManager.getRuntime().addRuntimeMeta(meta);
}

static void attachSmokeRuntime() {
    ConfigManager.addLivePage("Bluetooth",40);
    ConfigManager.addLiveCard("Bluetooth","Transport",40);
    ConfigManager.addLiveGroup("Bluetooth","Transport","Smoke test",40);
    ConfigManager.getRuntime().addRuntimeProvider("smoke",[](JsonObject& data) {
        data["scope"]="WiFi/UI/OTA + passive Victron BLE; BMS adapter missing";
        data["workerReady"]=bleWorkerReady;
        data["resetReason"]=static_cast<int>(esp_reset_reason());
    },5);
    addRuntimeField("smoke","scope","Test scope",1,false,true);
    addRuntimeField("smoke","workerReady","BLE worker ready",2,true);
    addRuntimeField("smoke","resetReason","Reset reason",3);
    addRuntimeField("bluetooth","enabled","Bluetooth enabled",10,true);
    addRuntimeField("bluetooth","state","Transport state",11);
    addRuntimeField("bluetooth","connected","GATT connected",12,true);
    addRuntimeField("bluetooth","scanning","Scanning",13,true);
    addRuntimeField("bluetooth","queueDrops","Queue drops",14);
    addRuntimeField("bluetooth","lastError","Last transport error",15);
    addRuntimeField("bluetooth","lateEvents","Suppressed late events",16);
    addRuntimeField("bluetooth","workerStackHighWaterBytes","Worker stack free (bytes)",17);
}

void setup() {
    Serial.begin(115200);
    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(APP_VERSION);
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
    // Keep the live dashboard focused on collector sources; serial health remains available.
    core.attachWiFi(ConfigManager,"Wi-Fi","Wi-Fi Settings",10);
    if(!attachSystemSettings()){Serial.println("[E] System settings initialization failed");return;}
    core.attachNtp(ConfigManager);
    bluetooth.attach(ConfigManager);
    attachSmokeRuntime();
    if(!victronReadout.attach(ConfigManager,bluetooth)){Serial.println("[E] Victron settings initialization failed");return;}
    if(!telemetryMqtt.attach(ConfigManager,MY_MQTT_BROKER_IP,MY_MQTT_BROKER_PORT,MY_MQTT_USERNAME,MY_MQTT_PASSWORD,SMOKE_LOCAL_SECRETS)){
        Serial.println("[E] MQTT settings initialization failed");return;
    }
    ConfigManager.getOTAManager().setLifecycleObserver([](const OtaLifecycleEvent& event) {
        telemetryMqtt.pauseForOta(event.stage==OtaStage::Starting||event.stage==OtaStage::Succeeded);
        if(event.stage==OtaStage::Starting || event.stage==OtaStage::Succeeded)
            bluetooth.requestPause(cm::BlePauseReason::Ota);
        else
            bluetooth.requestResume(cm::BlePauseReason::Ota);
    });
    ConfigManager.loadAll();
    telemetryMqtt.afterLoad();
    victronReadout.afterLoad();
    Preferences readoutPrefs;
    if(!readoutPrefs.begin("ConfigManager",false)){Serial.println("[E] Readout initialization failed");return;}
    const bool readoutReady=sbt::enableReadoutBluetoothOnce(readoutPrefs,bluetooth.settings().enabled,victronReadout.configured());
    readoutPrefs.end();
    if(!readoutReady){Serial.println("[E] Readout initialization failed");return;}
    setupNetworkDefaults();
#if defined(WIFI_FILTER_MAC_PRIORITY)
    ConfigManager.setAccessPointMacPriority(WIFI_FILTER_MAC_PRIORITY);
#endif
    // The configured SmartSolar peer uses passive advertisements; no GATT connection.
    // Do not initialize NimBLE separately; the CM worker owns its first radio operation.
    printHealth("before-ble");
    bleWorkerReady=bluetooth.begin();
    if(!bleWorkerReady)Serial.println("[W] BLE worker unavailable; WiFi/UI/OTA remain available");
    ConfigManager.startWebServer();
    printHealth("server-started");
    appReady=true;
    Serial.printf("[I] Collector runtime %s, reset=%d; passive Victron readout\n",APP_VERSION,static_cast<int>(esp_reset_reason()));
}

void loop() {
    if(!appReady){delay(100);return;}
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();
    const uint32_t now=millis();
    victronReadout.loop(now);
    bluetooth.loop(now);
    const auto sample=victronReadout.snapshot();
    telemetryMqtt.updateVictron(sample.values,sample.available);
    telemetryMqtt.loop(now);
    static uint32_t lastHealth=0;
    if(uint32_t(now-lastHealth)>=5000){lastHealth=now;printHealth("loop-alive");}
    delay(1);
}

void onWiFiConnected() {
    wifiServices.onConnected(ConfigManager,APP_NAME,systemSettings,core.ntp);
    Serial.println("[I] WiFi ready; UI/OTA available");
    printHealth("wifi-ready");
}
void onWiFiDisconnected() {printHealth("wifi-disconnected");wifiServices.onDisconnected();}
void onWiFiAPMode() {printHealth("wifi-ap-mode");wifiServices.onAPMode();}
