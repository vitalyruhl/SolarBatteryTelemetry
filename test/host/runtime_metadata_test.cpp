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
#include <functional>
#include "bms/BmsCommandPolicy.h"
#include <runtime/detail/RuntimeSnapshot.h>

struct RecordedField {
    RuntimeFieldBase base;
    RuntimeFieldKind kind;
    std::string unit;
    int precision = 0;
};
struct RecordingRuntime {
    std::vector<RecordedField> fields;
    struct Control { std::string group, key; std::function<bool()> getter; std::function<void(bool)> setter; };
    std::vector<Control> controls;
    std::vector<std::function<void()>> buttons;
    void registerRuntimeButton(const char*, const char*, std::function<void()> press) { buttons.push_back(press); }
    void registerRuntimeCheckbox(const char* source, const char* key, std::function<bool()> get, std::function<void(bool)> set) {
        controls.push_back({source, key, get, set});
    }
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
        else if constexpr (std::is_same_v<T, RuntimeCheckboxMeta>) kind = RuntimeFieldKind::Checkbox;
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
int main() {
    RecordingRuntime runtime;
    assert(sbt::metadata::registerSmokeFields(runtime));
    assert(sbt::metadata::registerTelemetryFields(runtime));
    assert(runtime.fields.size() == 56 && runtime.attempts == 56);
    std::map<RuntimeFieldKind, unsigned> counts;
    std::set<std::pair<std::string, std::string>> identities;
    std::set<std::string> connectivityKeys;
    for (const auto& field : runtime.fields) {
        ++counts[field.kind];
        assert(identities.emplace(field.base.sourceGroup.c_str(), field.base.key.c_str()).second);
        assert(!field.base.label.isEmpty());
        const std::string source = field.base.sourceGroup.c_str();
        const char* page = source == "bms" ? "BMS" : source == "victron" ? "Victron" : "Connectivity";
        assert(field.base.page == page);
        if (source != "bms" && source != "victron") {
            assert(field.base.card == "Connectivity");
            assert(connectivityKeys.emplace(field.base.key.c_str()).second);
        }

    }
    assert(counts[RuntimeFieldKind::Float] == 18);
    assert(counts[RuntimeFieldKind::Int] == 10);
    assert(counts[RuntimeFieldKind::Bool] == 12);
    assert(counts[RuntimeFieldKind::Text] == 14);
    assert(counts[RuntimeFieldKind::Checkbox] == 2);
    assert(counts[RuntimeFieldKind::Button] == 0);
    for (const auto& absent : {std::pair<const char*, const char*>{"telemetry", "root"}, {"telemetry", "rootValidation"}, {"telemetry", "ota"},
            {"bms", "availability"}, {"victron", "availability"}, {"bms", "cell_count"}, {"bms", "temperature_sensor_count"},
            {"victron", "pv_voltage_v"}, {"victron", "pv_current_a"}, {"victron", "battery_power_w"}})
        assert(!identities.count({absent.first, absent.second}));
    for (const char* source : {"bms", "victron"}) {
        expectKind(runtime, source, "reason", RuntimeFieldKind::Text);
        assert(find(runtime, source, "reason").base.label == "Source");
    }
    expectKind(runtime, "bms", "charge_enabled", RuntimeFieldKind::Bool);
    expectKind(runtime, "bms", "requested_charge_enabled", RuntimeFieldKind::Checkbox);
    expectKind(runtime, "bms", "discharge_enabled", RuntimeFieldKind::Bool);
    expectKind(runtime, "bms", "requested_discharge_enabled", RuntimeFieldKind::Checkbox);
    expectKind(runtime, "bluetooth", "enabled", RuntimeFieldKind::Bool);
    assert(find(runtime, "bluetooth", "enabled").base.label == "Bluetooth enabled");
    expectKind(runtime, "telemetry", "mqttEnabled", RuntimeFieldKind::Text);
    assert(find(runtime, "telemetry", "mqttEnabled").base.label == "MQTT enabled");
    expectKind(runtime, "bms", "temperature_1_c", RuntimeFieldKind::Float);
    expectKind(runtime, "bms", "temperature_2_c", RuntimeFieldKind::Float);
    assert(find(runtime, "bms", "temperature_1_c").base.label == "NTC 1 (Top)");
    assert(find(runtime, "bms", "temperature_2_c").base.label == "NTC 2 (Bottom)");
    expectKind(runtime, "bms", "charging", RuntimeFieldKind::Bool);
    expectKind(runtime, "bms", "discharging", RuntimeFieldKind::Bool);
    for (const char* key : {"connection", "mqttEnabled", "snapshot", "retirement"}) expectKind(runtime, "telemetry", key, RuntimeFieldKind::Text);
    for (const auto& channel : telemetry::channels) {
        const auto& field = find(runtime, telemetry::sourceName(channel.source), std::strchr(channel.suffix, '/') + 1);
        assert(field.base.label == channel.label);
        if (field.kind == RuntimeFieldKind::Float) {
            assert(field.unit == channel.unit);
            const std::string key = field.base.key.c_str();
            const bool cellVolts = key == "cell_voltage_min_v" || key == "cell_voltage_max_v" || key == "cell_voltage_delta_v";
            assert(field.precision == (cellVolts ? 3 : 2));
        }
    }
    for (const auto& entry : {std::pair<bool (*)(RecordingRuntime&), unsigned>{&sbt::metadata::registerSmokeFields<RecordingRuntime>, 11},
                             {&sbt::metadata::registerTelemetryFields<RecordingRuntime>, 45}}) {
        for (const unsigned failAt : {0u, entry.second / 2, entry.second - 1}) {
            RecordingRuntime failing;
            failing.failAfter = static_cast<int>(failAt);
            assert(!entry.first(failing));
            assert(failing.fields.size() == failAt && failing.attempts == failAt + 1);
        }
    }
    // [MOCKED!] Only runtime callback storage and observed samples; mailbox admission is real.
    bool observed[] = {true, false};
    bms::MosMailbox mailbox;
    bool enabled = false, fresh = false;uint32_t callbackNow=100;
    bms::MosRequest submitted{};
    const char* admission = "none";
    sbt::metadata::registerBmsCheckboxes(runtime, [&](unsigned i, bool state) {
            submitted.target = i == 0 ? bms::MosTarget::Charge : bms::MosTarget::Discharge;
            submitted.state = state; submitted.deadline = callbackNow+30000;
            std::snprintf(submitted.id, sizeof(submitted.id), "ui-fixture");
            admission = mailbox.offer(submitted, callbackNow, enabled, fresh, false);
        });
    assert(runtime.controls.size() == 2);
    assert(runtime.controls[0].group == "bms" && runtime.controls[0].key == "requested_charge_enabled");
    assert(runtime.controls[1].group == "bms" && runtime.controls[1].key == "requested_discharge_enabled");
    assert(!runtime.controls[0].getter && !runtime.controls[1].getter);
    // Real CM control merge must preserve provider-owned nullable observed states.
    JsonDocument document;
    JsonObject root = document.to<JsonObject>();
    for (const int state : {1, 0, -1}) {
        for (const char* key : {"requested_charge_enabled", "requested_discharge_enabled"}) {
            if (state < 0) root["bms"][key] = nullptr;
            else root["bms"][key] = state != 0;
        }
        cm::fillRuntimeControls(root, runtime.controls);
        for (const char* key : {"requested_charge_enabled", "requested_discharge_enabled"}) {
            if (state < 0) assert(root["bms"][key].isNull());
            else assert(root["bms"][key].is<bool>() && root["bms"][key].as<bool>() == (state != 0));
        }
    }
    runtime.controls[0].setter(false);
    assert(submitted.target == bms::MosTarget::Charge && !submitted.state);
    assert(std::string(admission) != "accepted" && !mailbox.pending() && observed[0]);
    enabled = true;
    runtime.controls[1].setter(true);
    assert(submitted.target == bms::MosTarget::Discharge && submitted.state);
    assert(std::string(admission) != "accepted" && !mailbox.pending() && !observed[1]);
    fresh = true;
    for (unsigned i = 0; i < 2; ++i) for (bool requested : {false, true}) {
        runtime.controls[i].setter(requested);
        assert(std::string(admission) == "accepted" && mailbox.pending());
        bms::MosRequest queued; assert(mailbox.take(queued));
        assert(queued.target == (i == 0 ? bms::MosTarget::Charge : bms::MosTarget::Discharge));
        assert(queued.state == requested && queued.deadline == callbackNow+30000 && std::string(queued.id) == "ui-fixture");
        mailbox.settle(false,callbackNow);callbackNow+=500;
        assert(observed[0] && !observed[1]);
    }

    unsigned pairMask=99;
    sbt::metadata::registerBmsPairButtons(runtime, [&](unsigned mask) { pairMask=mask; });
    assert(runtime.buttons.size()==4);
    for(unsigned mask=0;mask<4;++mask){runtime.buttons[mask]();assert(pairMask==mask);}
    std::puts("[MOCKED!] Runtime metadata and actual MOS controls passed");
}
