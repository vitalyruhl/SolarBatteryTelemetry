#pragma once
#include <cstring>

#ifndef SBT_ISOLATED_TEST_PROFILE
#define SBT_ISOLATED_TEST_PROFILE 0
#endif

#ifndef SBT_CONTROLLER_SCAN_TEST
#define SBT_CONTROLLER_SCAN_TEST 0
#endif

#ifndef SBT_FIELD_READOUT_TEST
#define SBT_FIELD_READOUT_TEST 0
#endif

#if SBT_CONTROLLER_SCAN_TEST && !SBT_ISOLATED_TEST_PROFILE
#error "Controller scan fixture requires the isolated test profile"
#endif

#if SBT_FIELD_READOUT_TEST && (!SBT_ISOLATED_TEST_PROFILE || SBT_CONTROLLER_SCAN_TEST)
#error "Field readout requires isolation and real peer reception"
#endif

namespace sbt::testprofile {
inline constexpr bool isolated = SBT_ISOLATED_TEST_PROFILE != 0;
inline constexpr bool fieldReadout = SBT_FIELD_READOUT_TEST != 0;
inline constexpr bool clearPeers = isolated && !fieldReadout;
#if SBT_FIELD_READOUT_TEST
inline constexpr char root[] = "SBTTest/C3";
inline constexpr char client[] = "SBTTest-C3-collector";
#else
inline constexpr char root[] = "SBTTest/COM8";
inline constexpr char client[] = "SBTTest-COM8-collector";
#endif
inline bool controlAllowed(bool enabled) { return enabled && !isolated; }
inline const char* mqttRoot(const char* configured) { return isolated ? root : configured; }
inline bool topicAllowed(const char* topic) {
    if (!isolated) return true;
    constexpr auto length = sizeof(root) - 1;
    return topic && std::strncmp(topic, root, length) == 0 && topic[length] == '/';
}
}
