// [MOCKED!] Only the Arduino String and metadata sink are host adapters.
// The application registration functions and CM public field definitions are real.
#include <string>
#include <cstdint>
class String {
    std::string value_;
public:
    String() = default;
    String(const char* value) : value_(value ? value : "") {}
    bool isEmpty() const { return value_.empty(); }
    String& operator+=(const String& other) { value_ += other.value_; return *this; }
    const char* c_str() const { return value_.c_str(); }
    bool operator==(const String& other) const { return value_ == other.value_; }
};
#include <runtime/RuntimeFieldTypes.h>
#include "runtime/AppRuntimeMetadata.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <set>
#include <type_traits>
#include <vector>

struct RecordedField {
    RuntimeFieldBase base;
    RuntimeFieldKind kind;
    std::string unit;
    int precision = 0;
};
struct RecordingRuntime {
    std::vector<RecordedField> fields;
    int failAfter = -1;
    unsigned attempts = 0;
    template<class T> RuntimeFieldId addRuntimeMeta(const T& meta) {
        ++attempts;
        if (failAfter == 0) return {};
        if (failAfter > 0) --failAfter;
        RuntimeFieldKind kind;
        if constexpr (std::is_same_v<T, RuntimeFloatMeta>) kind = RuntimeFieldKind::Float;
        else if constexpr (std::is_same_v<T, RuntimeIntMeta>) kind = RuntimeFieldKind::Int;
        else if constexpr (std::is_same_v<T, RuntimeBoolMeta>) kind = RuntimeFieldKind::Bool;
        else if constexpr (std::is_same_v<T, RuntimeTextMeta>) kind = RuntimeFieldKind::Text;
        else if constexpr (std::is_same_v<T, RuntimeButtonMeta>) kind = RuntimeFieldKind::Button;
        else static_assert(!sizeof(T), "The application must use a supported compact type");
        RecordedField field{meta.base, kind, "", 0};
        if constexpr (std::is_same_v<T, RuntimeFloatMeta> || std::is_same_v<T, RuntimeIntMeta>) field.unit = meta.unit.c_str();
        if constexpr (std::is_same_v<T, RuntimeFloatMeta>) field.precision = meta.precision;
        fields.push_back(field);
        return RuntimeFieldId{static_cast<uint32_t>(fields.size())};
    }
};
static const RecordedField& find(const RecordingRuntime& runtime, const char* source, const char* key) {
    for (const auto& field : runtime.fields)
        if (field.base.sourceGroup == source && field.base.key == key) return field;
    assert(false && "An original application field is missing");
    return runtime.fields.front();
}
static void expectKind(const RecordingRuntime& runtime, const char* source, const char* key, RuntimeFieldKind kind) {
    assert(find(runtime, source, key).kind == kind);
}
static bool allButtons(RecordingRuntime& runtime) {
    for (unsigned i = 0; i < 4; ++i) if (!sbt::metadata::registerBmsButton(runtime, i)) return false;
    return true;
}
int main() {
    RecordingRuntime runtime;
    assert(sbt::metadata::registerSmokeFields(runtime));
    assert(sbt::metadata::registerTelemetryFields(runtime));
    assert(allButtons(runtime));
    assert(runtime.fields.size() == 64 && runtime.attempts == 64);
    std::map<RuntimeFieldKind, unsigned> counts;
    std::set<std::pair<std::string, std::string>> identities;
    for (const auto& field : runtime.fields) {
        ++counts[field.kind];
        assert(identities.emplace(field.base.sourceGroup.c_str(), field.base.key.c_str()).second);
        assert(!field.base.label.isEmpty());
        const std::string source = field.base.sourceGroup.c_str();
        if (source == "smoke" || source == "bluetooth") {
            assert(field.base.page == "Bluetooth" && field.base.card == "Transport" && field.base.group == "Tests");
        } else {
            assert(source == "telemetry" || source == "bms" || source == "victron");
            const char* page = source == "bms" ? "BMS" : source == "victron" ? "Victron" : "MQTT";
            const char* card = source == "telemetry" ? "MQTT" : source.c_str();
            assert(field.base.page == page && field.base.card == card && field.base.group == card);
        }
    }
    assert(counts[RuntimeFieldKind::Float] == 19);
    assert(counts[RuntimeFieldKind::Int] == 12);
    assert(counts[RuntimeFieldKind::Bool] == 11);
    assert(counts[RuntimeFieldKind::Text] == 18);
    assert(counts[RuntimeFieldKind::Button] == 4);
    struct OriginalField { const char* source; const char* key; const char* label; int order; };
    const OriginalField smokeFields[] = {
        {"smoke", "scope", "Scope", 1}, {"smoke", "workerReady", "BLE worker ready", 2},
        {"smoke", "resetReason", "Reset reason", 3}, {"bluetooth", "enabled", "Bluetooth enabled", 10},
        {"bluetooth", "state", "Transport state", 11}, {"bluetooth", "connected", "GATT connected", 12},
        {"bluetooth", "scanning", "Scanning", 13}, {"bluetooth", "queueDrops", "Queue drops", 14},
        {"bluetooth", "lastError", "Last transport error", 15}, {"bluetooth", "lateEvents", "Suppressed late events", 16},
        {"bluetooth", "workerStackHighWaterBytes", "Worker stack free (bytes)", 17}
    };
    const OriginalField telemetryTextFields[] = {
        {"telemetry", "root", "Active base topic", 2}, {"telemetry", "connection", "MQTT connection", 3},
        {"telemetry", "enabled", "MQTT enabled", 4}, {"telemetry", "rootValidation", "Configuration validation", 5},
        {"telemetry", "snapshot", "Latest snapshot", 6}, {"telemetry", "ota", "MQTT OTA pause", 7},
        {"telemetry", "retirement", "Retained status cleanup", 8}, {"bms", "availability", "Source availability", 1},
        {"bms", "reason", "Source reason", 2}, {"victron", "availability", "Source availability", 1},
        {"victron", "reason", "Source reason", 2}
    };
    for (const auto& original : smokeFields) {
        const auto& field = find(runtime, original.source, original.key);
        assert(field.base.label == original.label && field.base.order == original.order);
    }
    for (const auto& original : telemetryTextFields) {
        const auto& field = find(runtime, original.source, original.key);
        assert(field.base.label == original.label && field.base.order == original.order);
    }
    for (const char* key : {"control_enabled", "control_ready", "control_pending", "control_admission", "control_result", "control_reason", "control_request_id", "sample_sequence", "sample_age_ms"}) {
        const auto& field = find(runtime, "bms", key);
        assert(field.base.label == key && field.base.order == 50);
    }
    expectKind(runtime, "smoke", "scope", RuntimeFieldKind::Text);
    expectKind(runtime, "smoke", "workerReady", RuntimeFieldKind::Bool);
    expectKind(runtime, "smoke", "resetReason", RuntimeFieldKind::Int);
    for (const char* key : {"enabled", "connected", "scanning"}) expectKind(runtime, "bluetooth", key, RuntimeFieldKind::Bool);
    for (const char* key : {"state", "queueDrops", "lastError", "lateEvents", "workerStackHighWaterBytes"}) expectKind(runtime, "bluetooth", key, RuntimeFieldKind::Int);
    for (const char* key : {"root", "connection", "enabled", "rootValidation", "snapshot", "ota", "retirement"}) expectKind(runtime, "telemetry", key, RuntimeFieldKind::Text);
    for (const char* source : {"bms", "victron"}) {
        expectKind(runtime, source, "availability", RuntimeFieldKind::Text);
        expectKind(runtime, source, "reason", RuntimeFieldKind::Text);
    }
    for (const char* key : {"control_enabled", "control_ready", "control_pending", "charging", "discharging", "charge_enabled", "discharge_enabled"}) expectKind(runtime, "bms", key, RuntimeFieldKind::Bool);
    for (const char* key : {"control_admission", "control_result", "control_reason", "control_request_id", "protection"}) expectKind(runtime, "bms", key, RuntimeFieldKind::Text);
    for (const char* key : {"sample_sequence", "sample_age_ms", "alarm_bits", "cell_count", "temperature_sensor_count"}) expectKind(runtime, "bms", key, RuntimeFieldKind::Int);
    expectKind(runtime, "victron", "charge_state", RuntimeFieldKind::Text);
    expectKind(runtime, "victron", "error_code", RuntimeFieldKind::Int);
    int channelOrder = 10;
    for (const auto& channel : telemetry::channels) {
        const auto& field = find(runtime, telemetry::sourceName(channel.source), std::strchr(channel.suffix, '/') + 1);
        assert(field.base.label == channel.label && field.base.order == channelOrder++);
        if (field.kind == RuntimeFieldKind::Float) {
            assert(field.unit == channel.unit);
            const std::string key = field.base.key.c_str();
            const bool cellVolts = key == "cell_voltage_min_v" || key == "cell_voltage_max_v" || key == "cell_voltage_delta_v";
            assert(field.precision == (cellVolts ? 3 : 2));
        }
    }
    const char* buttonKeys[] = {"charge_on", "charge_off", "discharge_on", "discharge_off"};
    const char* buttonLabels[] = {"Enable charge MOS", "Disable charge MOS", "Enable discharge MOS", "Disable discharge MOS"};
    for (unsigned i = 0; i < 4; ++i) {
        const auto& field = find(runtime, "bms", buttonKeys[i]);
        assert(field.kind == RuntimeFieldKind::Button && field.base.label == buttonLabels[i] && field.base.order == static_cast<int>(60 + i));
        assert(std::string(sbt::metadata::bmsButtonKeys[i]) == buttonKeys[i]);
    }
    assert(!sbt::metadata::registerBmsButton(runtime, 4));
    for (const auto& entry : {std::pair<bool (*)(RecordingRuntime&), unsigned>{&sbt::metadata::registerSmokeFields<RecordingRuntime>, 11},
                             {&sbt::metadata::registerTelemetryFields<RecordingRuntime>, 49}, {&allButtons, 4}}) {
        for (const unsigned failAt : {0u, entry.second / 2, entry.second - 1}) {
            RecordingRuntime failing;
            failing.failAfter = static_cast<int>(failAt);
            assert(!entry.first(failing));
            assert(failing.fields.size() == failAt && failing.attempts == failAt + 1);
        }
    }
    std::puts("[MOCKED!] Runtime sink contract passed: 64 production compact fields, routing/types/units/precision and failure propagation");
}
