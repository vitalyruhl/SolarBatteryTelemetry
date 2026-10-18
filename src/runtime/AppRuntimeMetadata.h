#pragma once
#include <runtime/RuntimeFieldTypes.h>
#include "../telemetry/TelemetryChannels.h"
#include <type_traits>

namespace sbt::metadata {
// Keep metadata separate from providers and control callbacks.
template<class Meta, class Runtime>
bool field(Runtime& runtime, const char* source, const char* key, const char* label,
           int order, const char* page, const char* card, const char* group,
           const char* unit = "", int precision = 2) {
    Meta meta;
    meta.base.sourceGroup = source;
    meta.base.key = key;
    meta.base.label = label;
    meta.base.order = order;
    meta.base.page = page;
    meta.base.card = card;
    meta.base.group = group;
    if constexpr (std::is_same_v<Meta, RuntimeFloatMeta> || std::is_same_v<Meta, RuntimeIntMeta>) meta.unit = unit;
    if constexpr (std::is_same_v<Meta, RuntimeFloatMeta>) meta.precision = precision;
    return static_cast<bool>(runtime.addRuntimeMeta(meta));
}

template<class Meta, class Runtime>
bool smokeField(Runtime& runtime, const char* source, const char* key, const char* label, int order) {
    return field<Meta>(runtime, source, key, label, order, "Bluetooth", "Transport", "Tests");
}

template<class Runtime> bool registerSmokeFields(Runtime& runtime) {
    return smokeField<RuntimeTextMeta>(runtime, "smoke", "scope", "Scope", 1) &&
        smokeField<RuntimeBoolMeta>(runtime, "smoke", "workerReady", "BLE worker ready", 2) &&
        smokeField<RuntimeIntMeta>(runtime, "smoke", "resetReason", "Reset reason", 3) &&
        smokeField<RuntimeBoolMeta>(runtime, "bluetooth", "enabled", "Bluetooth enabled", 10) &&
        smokeField<RuntimeIntMeta>(runtime, "bluetooth", "state", "Transport state", 11) &&
        smokeField<RuntimeBoolMeta>(runtime, "bluetooth", "connected", "GATT connected", 12) &&
        smokeField<RuntimeBoolMeta>(runtime, "bluetooth", "scanning", "Scanning", 13) &&
        smokeField<RuntimeIntMeta>(runtime, "bluetooth", "queueDrops", "Queue drops", 14) &&
        smokeField<RuntimeIntMeta>(runtime, "bluetooth", "lastError", "Last transport error", 15) &&
        smokeField<RuntimeIntMeta>(runtime, "bluetooth", "lateEvents", "Suppressed late events", 16) &&
        smokeField<RuntimeIntMeta>(runtime, "bluetooth", "workerStackHighWaterBytes", "Worker stack free (bytes)", 17);
}

template<class Meta, class Runtime>
bool telemetryField(Runtime& runtime, const char* source, const char* key, const char* label,
                    int order, const char* card = "MQTT", const char* unit = "", int precision = 2) {
    const char* page = std::strcmp(source, "bms") == 0 ? "BMS" : std::strcmp(source, "victron") == 0 ? "Victron" : "MQTT";
    return field<Meta>(runtime, source, key, label, order, page, card, card, unit, precision);
}

template<class Runtime> bool registerTelemetryFields(Runtime& runtime) {
    if (!telemetryField<RuntimeTextMeta>(runtime, "telemetry", "root", "Active base topic", 2) ||
        !telemetryField<RuntimeTextMeta>(runtime, "telemetry", "connection", "MQTT connection", 3) ||
        !telemetryField<RuntimeTextMeta>(runtime, "telemetry", "enabled", "MQTT enabled", 4) ||
        !telemetryField<RuntimeTextMeta>(runtime, "telemetry", "rootValidation", "Configuration validation", 5) ||
        !telemetryField<RuntimeTextMeta>(runtime, "telemetry", "snapshot", "Latest snapshot", 6) ||
        !telemetryField<RuntimeTextMeta>(runtime, "telemetry", "ota", "MQTT OTA pause", 7) ||
        !telemetryField<RuntimeTextMeta>(runtime, "telemetry", "retirement", "Retained status cleanup", 8)) return false;
    for (telemetry::Source source : {telemetry::Source::Bms, telemetry::Source::Victron}) {
        const char* name = telemetry::sourceName(source);
        if (!telemetryField<RuntimeTextMeta>(runtime, name, "availability", "Source availability", 1, name) ||
            !telemetryField<RuntimeTextMeta>(runtime, name, "reason", "Source reason", 2, name)) return false;
    }
    for (const char* key : {"control_enabled", "control_ready", "control_pending", "control_admission", "control_result", "control_reason", "control_request_id", "sample_sequence", "sample_age_ms"}) {
        const bool isBool = std::strcmp(key, "control_enabled") == 0 || std::strcmp(key, "control_ready") == 0 || std::strcmp(key, "control_pending") == 0;
        const bool isInt = std::strcmp(key, "sample_sequence") == 0 || std::strcmp(key, "sample_age_ms") == 0;
        const bool registered = isBool ? telemetryField<RuntimeBoolMeta>(runtime, "bms", key, key, 50, "bms") :
            isInt ? telemetryField<RuntimeIntMeta>(runtime, "bms", key, key, 50, "bms") :
            telemetryField<RuntimeTextMeta>(runtime, "bms", key, key, 50, "bms");
        if (!registered) return false;
    }
    // Preserve the existing 64 application fields; compact pools avoid the old legacy-vector growth cliff.
    int order = 10;
    for (const auto& channel : telemetry::channels) {
        const char* source = telemetry::sourceName(channel.source);
        const char* key = std::strchr(channel.suffix, '/') + 1;
        bool registered = false;
        switch (channel.type) {
        case telemetry::FieldType::Boolean:
            registered = telemetryField<RuntimeBoolMeta>(runtime, source, key, channel.label, order, source);
            break;
        case telemetry::FieldType::Real: {
            const bool cellVolts = std::strcmp(key, "cell_voltage_min_v") == 0 || std::strcmp(key, "cell_voltage_max_v") == 0 || std::strcmp(key, "cell_voltage_delta_v") == 0;
            registered = telemetryField<RuntimeFloatMeta>(runtime, source, key, channel.label, order, source, channel.unit, cellVolts ? 3 : 2);
            break;
        }
        case telemetry::FieldType::Unsigned:
            registered = telemetryField<RuntimeIntMeta>(runtime, source, key, channel.label, order, source, channel.unit);
            break;
        case telemetry::FieldType::State:
        case telemetry::FieldType::Protection:
            registered = telemetryField<RuntimeTextMeta>(runtime, source, key, channel.label, order, source);
            break;
        }
        if (!registered) return false;
        ++order;
    }
    return true;
}

inline constexpr const char* bmsButtonKeys[] = {"charge_on", "charge_off", "discharge_on", "discharge_off"};
template<class Runtime> bool registerBmsButton(Runtime& runtime, unsigned index) {
    if (index >= 4) return false;
    const char* labels[] = {"Enable charge MOS", "Disable charge MOS", "Enable discharge MOS", "Disable discharge MOS"};
    return field<RuntimeButtonMeta>(runtime, "bms", bmsButtonKeys[index], labels[index], static_cast<int>(60 + index), "BMS", "bms", "bms");
}
}
