#include <ConfigManager.h>
#include <RealMqttSettings.h>
#ifndef SBT_CM_LEGACY_ONLY
#include "telemetry/MqttUiSettings.h"
#include "settings/SettingsPersistence.h"
#include "telemetry/MqttConfigMailbox.h"
#endif
#include <cassert>
#include <cstdio>
#include <memory>

#ifndef SBT_CM_LEGACY_ONLY
// [MOCKED!] Credentials and broker values below are synthetic host-test fixtures.
static constexpr const char* host = "broker.invalid";
static constexpr const char* user = "fixture-user";
static constexpr const char* password = "fixture-password";
static constexpr const char* client = "fixture-client";

static std::vector<BaseSetting*> fields(telemetry::MqttUiSettings& ui) {
    return {&ui.enableMQTT, &ui.server, &ui.port, &ui.username, &ui.password,
            &ui.clientId, &ui.publishTopicBase, &ui.publishIntervalSec, &ui.listenIntervalMs, &ui.fullRefreshIntervalSec};
}
static void load(telemetry::MqttUiSettings& ui, Preferences& prefs) {
    for(auto* field : fields(ui)) field->load(prefs);
}
static String hashed(const char* hint) { return hashStringForStorage(String(hint)); }

namespace telemetry {
// Only the service network/registry shell is absent; these three methods are actual source.
struct ServiceSettingsHarness {
    std::unique_ptr<MqttUiSettings> ui_;
    ConfigMailbox mailbox_;
    ServiceSettingsHarness():ui_(std::make_unique<MqttUiSettings>(host,1884,user,password,true,client)) {}
#include "MqttServiceSettingsMethods.inc"
};
}
#endif
static void legacyFailure() {
    // Execute actual CM Settings construction and the former SBT temporary assignment.
    cm::MQTTManager::Settings oldUi;
    assert(!oldUi.enableMQTT.hasError());
    oldUi.enableMQTT = Config<bool>(ConfigOptions<bool>{.key="MQTTEnable", .name="Enable MQTT", .category="MQTT", .defaultValue=true});
    assert(oldUi.enableMQTT.hasError());
    ConfigManagerClass manager;
    assert(manager.addSetting(&oldUi.enableMQTT) == nullptr);
    assert(manager.settings.empty());
    std::puts("legacy_failure: real CM rejects former temporary Config reassignment");
}

#ifndef SBT_CM_LEGACY_ONLY
static void defaults() {
    // Actual canonical Settings must coexist with SBT UI settings without collisions.
    cm::MQTTManager::Settings transport;
    auto ui = std::make_unique<telemetry::MqttUiSettings>(host, 1884, user, password, true, client);
    ConfigManagerClass manager;
    assert(ui->attachTo(manager));
    assert(manager.settings.size() == 10);
    const char* hints[] = {"SBTMQTTEnable", "SBTMQTTHost", "SBTMQTTPort", "SBTMQTTUser", "SBTMQTTPass", "SBTMQTTClientId", "SBTMQTTBaseTopic", "SBTMQTTPubPer", "SBTMQTTListenMs", "SBTMQTTRefresh"};
    std::set<std::string> keys;
    auto list = fields(*ui);
    for(size_t i = 0; i < list.size(); ++i) {
        assert(!list[i]->hasError());
        assert(list[i]->shouldShowInWeb());
        assert(std::string(list[i]->getKey()) == hashed(hints[i]).c_str());
        assert(keys.insert(list[i]->getKey()).second);
    }
    assert(!transport.enableMQTT.hasError());
    assert(!transport.password.hasError());
    assert(!keys.count(transport.enableMQTT.getKey()));
    std::vector<std::string> churn(256, std::string(256, 'x'));
    for(size_t i = 0; i < list.size(); ++i) assert(std::string(list[i]->getKey()) == hashed(hints[i]).c_str());
    static_assert(!std::is_copy_constructible_v<telemetry::MqttUiSettings>);
    static_assert(!std::is_copy_assignable_v<telemetry::MqttUiSettings>);
    Preferences prefs;
    assert(ui->migrate(prefs) && prefs.writeAttempts == 0);
    load(*ui, prefs); // Real Config::load must retain final construction defaults.
    assert(ui->enableMQTT.get());
    assert(ui->server.get() == host && ui->port.get() == 1884);
    assert(ui->username.get() == user && ui->password.get() == password);
    assert(ui->clientId.get() == client);
    assert(ui->publishTopicBase.get() == "SolarBatteryTelemetry");
    assert(ui->publishIntervalSec.get() == 5.0f && ui->listenIntervalMs.get() == 0);
    assert(ui->fullRefreshIntervalSec.get() == 60);
    ui->server.set("edited.invalid"); ui->server.setDefault();
    assert(ui->server.get() == host);
    JsonDocument json;
    JsonObject root = json.to<JsonObject>();
    for(auto* field : list) field->toJSON(root);
    assert(root.size() == 10);
    assert(root[ui->password.getKey()]["value"].as<String>() == "***");
    assert(root[ui->password.getKey()]["isPassword"].as<bool>());
    assert(root[ui->publishTopicBase.getKey()]["value"].as<String>() == "SolarBatteryTelemetry");
    telemetry::MqttUiSettings duplicate(host, 1884, user, password, true, client);
    ConfigManagerClass rejected;
    assert(!duplicate.attachTo(rejected) && rejected.settings.empty());
    std::puts("defaults: ten unique real-CM registrations, final defaults, stable keys and password masking passed");
}

static void legacyValues(Preferences& prefs) {
    prefs.putBool(hashed("MQTTEnable").c_str(), false);
    prefs.putString(hashed("MQTTHost").c_str(), "legacy.invalid");
    prefs.putInt(hashed("MQTTPort").c_str(), 0);
    prefs.putString(hashed("MQTTUser").c_str(), "");
    prefs.putString(hashed("MQTTPass").c_str(), "");
    prefs.putString(hashed("MQTTClientId").c_str(), "legacy-client");
    prefs.putString(hashed("MQTTBaseTopic").c_str(), "house/battery");
    prefs.putFloat(hashed("MQTTPubPer").c_str(), 0.0f);
    prefs.putInt(hashed("MQTTListenMs").c_str(), 0);
}
static void migration() {
    telemetry::MqttUiSettings ui(host, 1884, user, password, true, client);
    Preferences prefs;
    legacyValues(prefs);
    assert(ui.migrate(prefs));
    load(ui, prefs);
    assert(!ui.enableMQTT.get());
    assert(ui.server.get() == "legacy.invalid" && ui.port.get() == 0);
    assert(ui.username.get() == "" && ui.password.get() == "");
    assert(ui.clientId.get() == "legacy-client" && ui.publishTopicBase.get() == "house/battery");
    assert(ui.publishIntervalSec.get() == 0.0f && ui.listenIntervalMs.get() == 0);
    const size_t writes = prefs.writeAttempts;
    assert(ui.migrate(prefs));
    assert(prefs.writeAttempts == writes);
    assert(prefs.isKey(hashed("MQTTPass").c_str()));
    assert(prefs.getString(hashed("MQTTBaseTopic").c_str()) == "house/battery");
    std::puts("migration: all nine legacy values, false/zero/empty, idempotence and old-key retention passed");
}
static void refreshHandoff() {
    telemetry::ServiceSettingsHarness service;
    auto& ui=*service.ui_;
    service.installCallbacks();
    service.captureInitial();
    telemetry::MqttConfig current;uint32_t seen=0;
    assert(service.mailbox_.take(seen,current)&&current.fullRefreshMs==60000);
    JsonDocument value;value.set(120);
    assert(ui.fullRefreshIntervalSec.fromJSON(value.as<JsonVariant>()));
    assert(service.mailbox_.take(seen,current)&&current.fullRefreshMs==120000&&current.publishMs==5000);
    for(int rejected:{4,3601}) {
        value.set(rejected);assert(!ui.fullRefreshIntervalSec.fromJSON(value.as<JsonVariant>()));
        assert(ui.fullRefreshIntervalSec.get()==120&&!service.mailbox_.take(seen,current));
    }
    value.set("60");assert(!ui.fullRefreshIntervalSec.fromJSON(value.as<JsonVariant>()));
    value.set(60.5);assert(!ui.fullRefreshIntervalSec.fromJSON(value.as<JsonVariant>()));
    value.set(nullptr);assert(!ui.fullRefreshIntervalSec.fromJSON(value.as<JsonVariant>()));
    Preferences prefs;
    ui.fullRefreshIntervalSec.save(prefs);
    ui.fullRefreshIntervalSec.set(60);
    ui.fullRefreshIntervalSec.load(prefs); // load intentionally does not call UI callbacks.
    service.captureInitial();assert(service.mailbox_.take(seen,current)&&current.fullRefreshMs==120000);
    prefs.putInt(ui.fullRefreshIntervalSec.getKey(),-100);ui.fullRefreshIntervalSec.load(prefs);
    service.captureInitial();assert(service.mailbox_.take(seen,current)&&current.fullRefreshMs==5000);
    prefs.putInt(ui.fullRefreshIntervalSec.getKey(),2147483647);ui.fullRefreshIntervalSec.load(prefs);
    service.captureInitial();assert(service.mailbox_.take(seen,current)&&current.fullRefreshMs==3600000);
    value.set(5);assert(ui.fullRefreshIntervalSec.fromJSON(value.as<JsonVariant>()));
    assert(service.mailbox_.take(seen,current)&&current.fullRefreshMs==5000);
    value.set(3600);assert(ui.fullRefreshIntervalSec.fromJSON(value.as<JsonVariant>()));
    assert(service.mailbox_.take(seen,current)&&current.fullRefreshMs==3600000);
    std::puts("refresh_handoff: real service startup/callbacks/load, strict JSON and effective persisted bounds passed");
}
static void newWins() {
    telemetry::MqttUiSettings ui(host, 1884, user, password, true, client);
    Preferences prefs;
    legacyValues(prefs);
    prefs.putBool(ui.enableMQTT.getKey(), true);
    prefs.putString(ui.server.getKey(), "");
    prefs.putInt(ui.port.getKey(), 1885);
    prefs.putString(ui.password.getKey(), "new-password");
    assert(ui.migrate(prefs));
    load(ui, prefs);
    assert(ui.enableMQTT.get() && ui.server.get() == "");
    assert(ui.port.get() == 1885 && ui.password.get() == "new-password");
    std::puts("new_wins: existing new values take precedence, including an explicit empty value");
}
static void migrationFailure() {
    telemetry::MqttUiSettings ui(host, 1884, user, password, true, client);
    Preferences prefs;
    legacyValues(prefs);
    prefs.failWrites = true;
    assert(!ui.migrate(prefs));
    assert(!prefs.isKey(ui.password.getKey()));
    assert(prefs.isKey(hashed("MQTTPass").c_str()));
    prefs.failWrites = false;
    prefs.reportSuccessWithoutWrite = true;
    assert(!ui.migrate(prefs));
    prefs.reportSuccessWithoutWrite = false;
    assert(ui.migrate(prefs));
    load(ui, prefs);
    assert(ui.password.get() == "");
    std::puts("migration_failure: failed write/readback propagates and retry preserves original values");
}
static void otaMissing() {
    Config<String> ota(ConfigOptions<String>{.key="OTAPass", .name="OTA Password", .category="System", .defaultValue=String(""), .isPassword=true});
    Preferences prefs;
    assert(sbt::initializeSettingIfMissing(prefs, ota, String("fixture-ota")));
    assert(prefs.isKey(ota.getKey()));
    ota.load(prefs);
    assert(ota.get() == "fixture-ota");
    const size_t writes = prefs.writeAttempts;
    assert(sbt::initializeSettingIfMissing(prefs, ota, String("replacement")));
    assert(prefs.writeAttempts == writes);
    Config<String> failed(ConfigOptions<String>{.key="HostFailedOTA", .name="Failed OTA", .category="System", .defaultValue=String("")});
    prefs.failWrites = true;
    assert(!sbt::initializeSettingIfMissing(prefs, failed, String("fixture-ota")));
    assert(!prefs.isKey(failed.getKey()));
    std::puts("ota_missing: missing OTA provisioned, existing value retained and failed write detected");
}
static void otaEmpty() {
    Config<String> ota(ConfigOptions<String>{.key="OTAPass", .name="OTA Password", .category="System", .defaultValue=String(""), .isPassword=true});
    Preferences prefs;
    prefs.putString(ota.getKey(), "");
    const size_t writes = prefs.writeAttempts;
    prefs.failWrites = true;
    assert(sbt::initializeSettingIfMissing(prefs, ota, String("fixture-ota")));
    assert(prefs.writeAttempts == writes);
    ota.load(prefs);
    assert(ota.get() == "" && !ota.hasError());
    ConfigManagerClass manager;
    assert(manager.addSetting(&ota) == &ota);
    std::puts("ota_empty: explicit persisted empty password survives without a write or Config reconstruction");
}
static void readoutEnableOnce() {
    Config<bool> enabled(ConfigOptions<bool>{.key="BluetoothEnable",.name="Enable Bluetooth",.category="Bluetooth",.defaultValue=false});
    Preferences prefs;enabled.load(prefs);
    const auto writes=prefs.writeAttempts;
    assert(sbt::enableReadoutBluetoothOnce(prefs,enabled,false));
    assert(prefs.writeAttempts==writes&&!enabled.get());
    assert(sbt::enableReadoutBluetoothOnce(prefs,enabled,true)&&enabled.get());
    enabled.set(false);enabled.save(prefs);
    const auto savedWrites=prefs.writeAttempts;
    assert(sbt::enableReadoutBluetoothOnce(prefs,enabled,true)&&!enabled.get());
    assert(prefs.writeAttempts==savedWrites);
    std::puts("readout_enable_once: first configured upgrade enables BLE, later user disable retained");
}

#endif
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string scenario = argv[1];
    if(scenario == "legacy_failure") legacyFailure();
#ifndef SBT_CM_LEGACY_ONLY
    else if(scenario == "defaults") defaults();
    else if(scenario == "migration") migration();
    else if(scenario == "new_wins") newWins();
    else if(scenario == "migration_failure") migrationFailure();
    else if(scenario == "ota_missing") otaMissing();
    else if(scenario == "ota_empty") otaEmpty();
    else if(scenario == "readout_enable_once") readoutEnableOnce();
    else if(scenario == "refresh_handoff") refreshHandoff();
#endif
    else assert(false && "Unknown scenario");
}