#include "smoke/IsolatedTestProfile.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>

int main() {
    static_assert(sbt::testprofile::isolated);
    static_assert(sbt::testprofile::fieldReadout);
    static_assert(!sbt::testprofile::clearPeers);
    assert(!sbt::testprofile::controlAllowed(true));
    assert(!sbt::testprofile::controlAllowed(false));
    assert(std::strcmp(sbt::testprofile::mqttRoot("SolarBatteryTelemetry"), "SBTTest/C3") == 0);
    assert(std::strcmp(sbt::testprofile::client, "SBTTest-C3-collector") == 0);
    for (const char* topic : {"SolarBatteryTelemetry/status", "homeassistant/sensor/config",
                             "SBTTest/COM8/status", "SBTTest/C30/status", "SBTTest/C3"})
        assert(!sbt::testprofile::topicAllowed(topic));
    assert(sbt::testprofile::topicAllowed("SBTTest/C3/bms/health"));
    assert(sbt::testprofile::topicAllowed("SBTTest/C3/victron/health"));
    std::puts("PASS: field readout preserves peers, isolates MQTT, and denies controls");
}
