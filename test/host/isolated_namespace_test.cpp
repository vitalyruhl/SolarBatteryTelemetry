// [MOCKED!] Network sink only; the real publisher resolves its initial namespace.
#include "smoke/IsolatedTestProfile.h"
#include "telemetry/TelemetryPublisher.h"
#include "telemetry/MqttLoopPolicy.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
struct Wire final : telemetry::Transport {
    bool online = false;
    std::string configured;
    std::vector<std::string> topics;
    bool connected() const override { return online; }
    bool publish(const char* topic, const char*, bool) override {
        assert(sbt::testprofile::topicAllowed(topic));
        topics.emplace_back(topic); return true;
    }
    void disconnect() override { online = false; }
    void configure(const char* root) override { configured = sbt::testprofile::mqttRoot(root); }
};
int main() {
    assert(!sbt::testprofile::controlAllowed(false));
    assert(sbt::testprofile::controlAllowed(true) == !sbt::testprofile::isolated);
    const char* root = sbt::testprofile::mqttRoot("SolarBatteryTelemetry");
    if constexpr(sbt::testprofile::isolated) {
        assert(std::strcmp(root,"SBTTest/COM8") == 0);
        for(const char* topic : {"SolarBatteryTelemetry/status","homeassistant/sensor/config","SBTTest/COM80/status","SBTTest/COM8"})
            assert(!sbt::testprofile::topicAllowed(topic));
        for(const char* topic : {"SBTTest/COM8/status","SBTTest/COM8/bms/emergency_off/set","SBTTest/COM8/System-Info/ESP"})
            assert(sbt::testprofile::topicAllowed(topic));
    }
    telemetry::Publisher publisher;
    Wire wire;
    assert(publisher.requestRoot(root,std::strlen(root)));
    bool connected = false;
    telemetry::runPreparedMqttLoop(publisher,100,wire,true,[]{return false;},[&] {
        assert(std::strcmp(publisher.root(),root) == 0);
        wire.online=true;publisher.onConnected();connected=true;
    });
    assert(connected);
    publisher.tick(200,wire);
    assert(!wire.topics.empty());
    for(const auto& topic : wire.topics) assert(topic.rfind(std::string(root)+"/",0)==0);
    std::puts("PASS: namespace is prepared before first connect; publications and control policy stay profile-bound");
}
