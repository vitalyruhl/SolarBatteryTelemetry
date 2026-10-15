#include <Arduino.h>
#include <ConfigManager.h>
#include <bluetooth/BluetoothManager.h>
#include <core/CoreSettings.h>
#include <core/CoreWiFiServices.h>
#include <esp_system.h>
#include "settings.h"
#include "smoke/SmokePolicy.h"

static cm::BluetoothManager bluetooth;
static auto& core=cm::CoreSettings::instance();
static auto& wifiSettings=core.wifi;
static auto& systemSettings=core.system;
static cm::CoreWiFiServices wifiServices;
static bool bleWorkerReady=false; // Written before web-server startup, then immutable.

static void attachSystemSettings() {
    // Seed the existing OTA password's default before registration/loadAll.
    // Persisted UI edits, including an intentionally empty password, win on later boots.
    systemSettings.otaPassword=Config<String>(ConfigOptions<String>{
        .key="OTAPass", .name="OTA Password", .category=cm::CoreCategories::System,
        .defaultValue=String(OTA_PASSWORD), .showInWeb=true, .isPassword=true, .sortOrder=2});
    ConfigManager.setCategoryLayoutOverride(cm::CoreCategories::System,cm::CoreCategories::System,
        cm::CoreCategories::System,"System Settings",20);
    ConfigManager.addSettingsPage(cm::CoreCategories::System,20);
    ConfigManager.addSettingsGroup(cm::CoreCategories::System,cm::CoreCategories::System,"System Settings",20);
    ConfigManager.addSetting(&systemSettings.allowOTA);
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
        data["scope"]="WiFi/UI/OTA + BLE worker only; peers missing";
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
    ConfigManager.enableBuiltinSystemProvider();
    core.attachWiFi(ConfigManager,"WLAN","WLAN-Einstellungen",10);
    attachSystemSettings();
    core.attachNtp(ConfigManager);
    bluetooth.attach(ConfigManager);
    attachSmokeRuntime();
    ConfigManager.getOTAManager().setLifecycleObserver([](const OtaLifecycleEvent& event) {
        if(event.stage==OtaStage::Starting || event.stage==OtaStage::Succeeded)
            bluetooth.requestPause(cm::BlePauseReason::Ota);
        else
            bluetooth.requestResume(cm::BlePauseReason::Ota);
    });
    ConfigManager.loadAll();
    setupNetworkDefaults();
#if defined(WIFI_FILTER_MAC_PRIORITY)
    ConfigManager.setAccessPointMacPriority(WIFI_FILTER_MAC_PRIORITY);
#endif
    // No guessed peer/profile: the optional transport remains Disabled/Idle.
    // Do not initialize NimBLE separately; the CM worker owns its first radio operation.
    bleWorkerReady=bluetooth.begin();
    if(!bleWorkerReady)Serial.println("[W] BLE worker unavailable; WiFi/UI/OTA remain available");
    ConfigManager.startWebServer();
    Serial.printf("[I] Smoke runtime %s, reset=%d; device telemetry inactive\n",APP_VERSION,static_cast<int>(esp_reset_reason()));
}

void loop() {
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();
    bluetooth.loop(millis());
    delay(1);
}

void onWiFiConnected() {
    wifiServices.onConnected(ConfigManager,APP_NAME,systemSettings,core.ntp);
    Serial.println("[I] WiFi ready; UI/OTA available");
}
void onWiFiDisconnected() {wifiServices.onDisconnected();}
void onWiFiAPMode() {wifiServices.onAPMode();}
