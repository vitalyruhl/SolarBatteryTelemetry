"""Focused SBT settings regression against extracted, unmodified CM definitions.

Only Arduino/Preferences and the manager's surrounding platform/layout facade are
[MOCKED!]. ConfigOptions, hash helpers, BaseSetting, Config<T>, addSetting(), and
MQTT Settings construction are copied verbatim from the configured local CM.
No firmware or credentials are printed, and no hardware operation is performed.
"""
from __future__ import annotations
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def registry_cm_path() -> Path:
    supplied = None
    if '--config-manager-path' in sys.argv:
        supplied = sys.argv[sys.argv.index('--config-manager-path') + 1]
    cm = Path(supplied) if supplied else ROOT / '.pio/registry-settings/libdeps/usb/ESP32 Configuration Manager'
    metadata = json.loads((cm / 'library.json').read_text(encoding='utf-8-sig'))
    if metadata.get('name') != 'ESP32 Configuration Manager' or metadata.get('version') != '4.9.0':
        raise RuntimeError('Host validation requires the published CM 4.9.0 package')
    return cm


def between(source: str, start: str, end: str) -> str:
    if source.count(start) != 1:
        raise RuntimeError(f"CM extraction start marker changed: {start}")
    begin = source.index(start)
    finish = source.find(end, begin + len(start))
    if finish < 0:
        raise RuntimeError(f"CM extraction end marker changed: {end}")
    return source[begin:finish]


def method(source: str, signature: str) -> str:
    if source.count(signature) != 1:
        raise RuntimeError(f"CM method extraction marker changed: {signature}")
    begin = source.index(signature)
    opening = source.index("{", begin)
    depth = 1
    cursor = opening + 1
    while depth and cursor < len(source):
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    if depth:
        raise RuntimeError("Unbalanced CM extraction")
    return source[begin:cursor]


def main() -> None:
    cm = registry_cm_path()
    source = (cm / "src/ConfigManager.h").read_text(encoding="utf-8-sig")
    mqtt = (cm / "src/mqtt/MQTTManager.h").read_text(encoding="utf-8-sig")
    legacy_only = "--legacy-only" in sys.argv[1:]
    required = [ROOT / "src/telemetry/MqttUiSettings.h", ROOT / "src/settings/SettingsPersistence.h"]
    if not legacy_only and not all(path.is_file() for path in required):
        raise RuntimeError("Settings regression is RED: product settings headers are missing")
    json_candidates = sorted((ROOT / ".pio").glob("**/libdeps/*/ArduinoJson/src/ArduinoJson.h"))
    if not json_candidates:
        raise RuntimeError("Installed ArduinoJson headers are required; run the existing project build first")
    json_include = json_candidates[0].parent
    compiler = Path(os.environ.get("SBT_HOST_CXX", "C:/Program Files/LLVM/bin/clang++.exe"))
    if not compiler.is_file():
        raise RuntimeError("Host clang++ is unavailable")
    parts = [
        '#pragma once\n#include "cm_platform_adapter.h"\n#define CM_ENABLE_LOGGING 0\n#define CM_ENABLE_VERBOSE_LOGGING 0\n#define CM_CORE_LOG(...) do {} while(0)\n',
        between(source, "template <typename T>\nstruct ConfigOptions", "class ConfigManagerClass;"),
        between(source, "enum class SettingType", "using LogCallback"),
        between(source, "inline String generateKeyFromNameAndCategory", "struct ConfigRequestContext"),
        between(source, "class BaseSetting {", "// Config<T> template class"),
        between(source, "template <typename T>\nclass Config : public BaseSetting", "// Visibility helper factories"),
        # [MOCKED!] Only the platform/layout shell; addSetting is actual CM source.
        'class ConfigManagerClass { public:\nstd::vector<BaseSetting*> settings;\n',
        method(source, "BaseSetting* addSetting(BaseSetting* setting)"),
        '\nvoid registerSettingPlacement(BaseSetting*) {}\n'
        'template<class... T> void setCategoryLayoutOverride(T...) {}\n'
        'template<class... T> void addSettingsPage(T...) {}\n'
        'template<class... T> void addSettingsGroup(T...) {}\n'
        'static void log_message(const char*, ...) {}\n};\n',
    ]
    settings = between(mqtt, "  struct Settings {", "  Settings& settings()")
    ctor = between(mqtt, "inline MQTTManager::Settings::Settings()", "inline MQTTManager::MQTTManager()")
    fixture = '#pragma once\n#include <ConfigManager.h>\nnamespace cm { class MQTTManager { public:\n' + settings + '\n};\n' + ctor + '\n}\n'
    output_parent = ROOT / ".pio/host-tests"
    output_parent.mkdir(parents=True, exist_ok=True)
    # TemporaryDirectory verifies a project-local unique absolute path and cleans it.
    with tempfile.TemporaryDirectory(prefix="settings-real-cm-", dir=output_parent) as directory:
        out = Path(directory).resolve()
        if out.parent != output_parent.resolve() or not out.name.startswith("settings-real-cm-"):
            raise RuntimeError("Unexpected temporary output target")
        (out / "ConfigManager.h").write_text("\n".join(parts), encoding="utf-8")
        (out / "RealMqttSettings.h").write_text(fixture, encoding="utf-8")
        service = (ROOT / "src/telemetry/MqttService.h").read_text(encoding="utf-8")
        methods = [method(service, signature) for signature in (
            "void captureText(TextField field,const String& value)", "void captureInitial()", "void installCallbacks()")]
        (out / "MqttServiceSettingsMethods.inc").write_text("\n".join(methods), encoding="utf-8")

        for name in ("Arduino.h", "Preferences.h"):
            (out / name).write_text('#pragma once\n#include "cm_platform_adapter.h"\n', encoding="utf-8")
        executable = out / "settings_registration_test.exe"
        command = [str(compiler), "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-deprecated-declarations", "-Wno-reorder-ctor", "-Wno-unused-parameter", "-DCM_ENABLE_SETTINGS_UI=1", "-DCM_ENABLE_LIVE_UI=0",
                   "-I", str(out), "-I", str(ROOT / "test/host"), "-I", str(ROOT / "src"), "-I", str(json_include),
                   str(ROOT / "test/host/settings_registration_test.cpp"), "-o", str(executable)]
        if legacy_only:
            command.insert(1, "-DSBT_CM_LEGACY_ONLY")
        isolated_only = '--isolated-only' in sys.argv
        if isolated_only:
            command.insert(1, '-DSBT_ISOLATED_TEST_PROFILE=1')
        subprocess.run(command, check=True)
        scenarios = ("isolated_namespace",) if isolated_only else ("legacy_failure",) if legacy_only else ("legacy_failure", "defaults", "migration", "new_wins", "migration_failure", "ota_missing", "ota_empty", "readout_enable_once", "refresh_handoff")
        for scenario in scenarios:
            subprocess.run([str(executable), scenario], check=True)
    print(f"Settings regression: {len(scenarios)} real-CM scenarios passed; temporary artifacts removed")


if __name__ == "__main__":
    main()
