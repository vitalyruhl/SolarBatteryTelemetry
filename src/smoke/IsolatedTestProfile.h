#pragma once
#include <cstring>

#ifndef SBT_ISOLATED_TEST_PROFILE
#define SBT_ISOLATED_TEST_PROFILE 0
#endif

namespace sbt::testprofile {
inline constexpr bool isolated = SBT_ISOLATED_TEST_PROFILE != 0;
inline constexpr char root[] = "SBTTest/COM8";
inline constexpr char client[] = "SBTTest-COM8-collector";
inline bool controlAllowed(bool enabled) { return enabled && !isolated; }
inline const char* mqttRoot(const char* configured) { return isolated ? root : configured; }
inline bool topicAllowed(const char* topic) {
    if (!isolated) return true;
    constexpr auto length = sizeof(root) - 1;
    return topic && std::strncmp(topic, root, length) == 0 && topic[length] == '/';
}
}
