"""[MOCKED!] Compile the actual SBT callback against a terminal worker facade."""
from pathlib import Path
import re, subprocess, tempfile

root=Path(__file__).resolve().parents[2]
source=(root/'src/main.cpp').read_text()
match=re.search(r'ConfigManager\.getOTAManager\(\)\.setRebootPreparation\(\[\]\(\)\s*\{(.*?)\}\);',source,re.S)
assert match, 'Missing actual nonblocking BLE reboot preparation callback'
assert source.count('setRebootPreparation(')==1, 'Register exactly one preparation callback'
assert source.index('void setup()') < match.start() < source.index('ConfigManager.startWebServer();')
body=match.group(1)
assert not re.search(r'\b(?:delay|vTaskDelay|while|for|ESP|NimBLEDevice|esp_bt_controller_disable)\b',body), 'App callback must not block, reboot, or deinitialize the driver'
assert body.index('requestShutdownForRestart') < body.index('isShutdownForRestartComplete')
fixture="""#include <cassert>
struct Worker {
 bool complete=false; int requested=0;
 void requestShutdownForRestart(){++requested;}
 bool isShutdownForRestartComplete()const{return complete;}
} bluetooth;
int main(){auto prepare=[](){BODY};
 assert(!prepare()); assert(bluetooth.requested==1);
 assert(!prepare()); assert(bluetooth.requested==2);
 bluetooth.complete=true; assert(prepare()); assert(bluetooth.requested==3);
 assert(prepare()); assert(bluetooth.requested==4);
}
""".replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='sbt-reboot-wire-',dir='C:/Temp') as tmp:
 cpp=Path(tmp)/'test.cpp';exe=Path(tmp)/'test.exe'
 cpp.write_text(fixture)
 subprocess.run(['C:/Program Files/LLVM/bin/clang++.exe','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
ini=(root/'platformio.ini').read_text()
common=ini.split('[env]')[1].split('[env:usb]')[0]
assert 'CM_TEST_RESTART_PHASES' not in common, 'Production must not enable phase markers'
normaltest=ini.split('[env:testesp]')[1].split('[env:testesp_panic]')[0]
assert '-DCM_TEST_RESTART_PHASES=1' in normaltest
print('[MOCKED!] Actual setup registration, pending/ACK callback, no SDK calls, and test-only phase markers passed.')
