"""Exercise actual Arduino baseline and SBT replacement with [MOCKED!] SDK affinity checks."""
from pathlib import Path
import os
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
FRAMEWORK=Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))/'packages/framework-arduinoespressif32'
source=(FRAMEWORK/'cores/esp32/esp32-hal-time.c').read_text()
start=source.index('void configTzTime(')
end=source.index('\nbool getLocalTime',start)
out=ROOT/'.pio/host-tests';out.mkdir(parents=True,exist_ok=True)
(out/'raw_ntp_baseline.h').write_text(source[start:end])
compiler=os.environ.get('SBT_HOST_CXX','C:/Program Files/LLVM/bin/clang++.exe')
command=[compiler,'-std=c++17','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations','-I',str(ROOT/'test/host/ntp_stubs'),'-I',str(out),str(ROOT/'test/host/ntp_thread_safety_test.cpp')]
baseline=out/'raw_ntp_baseline.exe'
subprocess.run(command+['-DSBT_RAW_NTP_BASELINE=1','-o',str(baseline)],check=True)
failed=subprocess.run([str(baseline)],capture_output=True,text=True)
if failed.returncode==0 or 'raw SNTP called outside TCP/IP task' not in failed.stderr:
    raise RuntimeError('Arduino baseline did not fail at the intended raw SNTP affinity check: '+failed.stderr)
print('RED confirmed: unchanged Arduino configTzTime calls raw SNTP outside TCP/IP task')
if '--baseline-only' in sys.argv:sys.exit(0)
candidate=out/'safe_ntp_test.exe'
subprocess.run(command+[str(ROOT/'src/network/SafeNtp.cpp'),'-o',str(candidate)],check=True)
subprocess.run([str(candidate)],check=True)
