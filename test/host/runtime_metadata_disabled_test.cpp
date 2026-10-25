// [MOCKED!] The sink matches CM 4.9.0's disabled Live API; application registration is real.
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
struct DisabledRuntime {
    unsigned calls = 0;
    template<class T> RuntimeFieldId addRuntimeMeta(const T&) { ++calls; return {}; }
};
int main() {
    DisabledRuntime runtime;
    assert(sbt::metadata::registerSmokeFields(runtime));
    assert(sbt::metadata::registerTelemetryFields(runtime));
    assert(runtime.calls == 0);
    std::puts("PASS: Settings-only startup accepts CM's disabled Live API without registering descriptors");
}
