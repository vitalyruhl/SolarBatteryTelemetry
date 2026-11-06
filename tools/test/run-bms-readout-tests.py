"""Real BmsReadout/Parser/Cycle/Policy; [MOCKED!] platform/BLE/runtime UI only."""
from pathlib import Path
import configparser, importlib.util, os, subprocess, tempfile, sys
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("settings_harness",Path(__file__).with_name("run-settings-registration-tests.py"))
helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
def main():
 cm=helper.registry_cm_path()
 source=(cm/"src/ConfigManager.h").read_text(encoding="utf-8-sig")
 parts=['#pragma once\n#include "bms_readout_platform.h"\n#define CM_ENABLE_LOGGING 0\n#define CM_ENABLE_VERBOSE_LOGGING 0\n#define CM_CORE_LOG(...) do {} while(0)\n']
 for start,end in [('template <typename T>\nstruct ConfigOptions','class ConfigManagerClass;'),('enum class SettingType','using LogCallback'),('inline String generateKeyFromNameAndCategory','struct ConfigRequestContext'),('class BaseSetting {','// Config<T> template class'),('template <typename T>\nclass Config : public BaseSetting','// Visibility helper factories')]:parts.append(helper.between(source,start,end))
 parts+=['class ConfigManagerClass { public: std::vector<BaseSetting*> settings;HostRuntime runtime;HostRuntime& getRuntime(){return runtime;}',helper.method(source,'BaseSetting* addSetting(BaseSetting* setting)'),'''void registerSettingPlacement(BaseSetting*){}
 template<class...T> void setCategoryLayoutOverride(T...){}
 template<class...T> void addSettingsCard(T...){}
 template<class...T> void addSettingsGroup(T...){}
 template<class...T> void addToSettingsGroup(T...){}
 static void log_message(const char*,...){}
 };''']
 parts.append('inline bool hostSaveOkay=true,hostWriteOkay=true;inline Preferences hostGatePrefs;template<class T> bool Config<T>::save(const T& value){if(!hostSaveOkay)return false;set(value);hostGatePrefs.failWrites=!hostWriteOkay;save(hostGatePrefs);return true;}')
 parent=ROOT/".pio/host-tests";parent.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix="bms-readout-",dir=parent) as directory:
  out=Path(directory).resolve()
  if out.parent!=parent.resolve() or not out.name.startswith('bms-readout-'):raise RuntimeError('Unexpected temporary path')
  (out/'ConfigManager.h').write_text('\n'.join(parts),encoding='utf-8')
  (out/'BmsReadout.h').write_bytes((ROOT/'src/bms/BmsReadout.h').read_bytes())
  (out/'runtime').mkdir();(out/'runtime/AppRuntimeMetadata.h').write_text('#pragma once\n#include <ConfigManager.h>\nnamespace sbt::metadata { template<class R,class F> void registerBmsCheckboxes(R&,F){} template<class R,class F> void registerBmsPairButtons(R&,F){} }\n')
  (out/'victron').mkdir()
  victron=(ROOT/'src/victron/VictronReadout.h').read_text()
  validated=helper.between(victron,'class ValidatedText final','class Readout {')
  (out/'victron/VictronReadout.h').write_text('#pragma once\n#include <ConfigManager.h>\n#include "victron/VictronConfigMailbox.h"\nnamespace victron {\n'+validated+'\n}\n')
  json_candidates=sorted((ROOT/'.pio').glob('**/libdeps/*/ArduinoJson/src/ArduinoJson.h'))
  if not json_candidates:raise RuntimeError('Installed ArduinoJson headers are required')
  json=json_candidates[0].parent
  compiler=os.environ.get('SBT_HOST_CXX','C:/Program Files/LLVM/bin/clang++.exe')
  exe=out/'bms_readout_test.exe'
  command=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations','-Wno-reorder-ctor','-Wno-unused-parameter']
  for include in [out,ROOT/'test/host',ROOT/'src',ROOT/'src/bms',json]:command+=['-I',str(include)]
  arguments=sys.argv[1:]
  if '--config-manager-path' in arguments:
   at=arguments.index('--config-manager-path');arguments=arguments[:at]+arguments[at+2:]
  if '--isolated-only' in arguments or '--field-only' in arguments:
   profile_flags=['-DSBT_ISOLATED_TEST_PROFILE=1']
   if '--field-only' in arguments:profile_flags+=['-DSBT_FIELD_READOUT_TEST=1']
   subprocess.run(command+profile_flags+[str(ROOT/'test/host/bms_isolated_profile_test.cpp'),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
   return
  subprocess.run(command+[str(ROOT/'test/host/bms_readout_test.cpp'),'-o',str(exe)],check=True)
  scenarios=arguments or ["gate_buffered_setting","gate_buffered_mqtt","gate_settings","gate_fence","emergency_alarm","emergency_clear","emergency_disabled","emergency_software","cached_manual","disconnect","reconnect","query_timeout","completion_error","invalid_response","generation","write_error","short_timeout","long_timeout","rollover","ota","mac_change","mac_empty","master_disable","control_age","partial_pair","settings","historical_settings","short_control","queue_short","queue_pause","queue_stale","command_settling"]
  for scenario in scenarios:subprocess.run([str(exe),scenario],check=True)
 print('BMS readout behavioral tests passed; temporary artifacts removed')
if __name__=='__main__':main()
