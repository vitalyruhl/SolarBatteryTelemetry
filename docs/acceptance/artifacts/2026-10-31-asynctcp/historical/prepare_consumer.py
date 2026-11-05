from pathlib import Path
import json,re,shutil,subprocess,tarfile
ROOT=Path(__file__).resolve().parent
SOURCE=Path(r'C:\Daten\_Codding\ESP32\SolarBatteryTelemetry')
DEST=ROOT/'sbt-compat-consumer'
assert not DEST.exists()
archive=ROOT/'sbt-main.tar'
subprocess.run(['git','-C',str(SOURCE),'archive','--format=tar','--output',str(archive),'main'],check=True)
DEST.mkdir()
with tarfile.open(archive) as tar:
 for m in tar.getmembers():
  target=(DEST/m.name).resolve()
  if not target.is_relative_to(DEST.resolve()) or m.issym() or m.islnk():raise RuntimeError('Unsafe archive member')
 tar.extractall(DEST,filter='data')
freeze='.local/cm-ble-reboot-4.10.5-final-package'
shutil.copytree(SOURCE/freeze,DEST/freeze)
# Build-local ignored configuration is copied without printing any content.
private_copies=[]
for relative in ['platformio.local.ini','src/secret/secrets.h','src/secret/wifiSecret.h','secret/secrets.h','secret/wifiSecret.h','include/secrets.h','include/wifiSecret.h']:
 src=SOURCE/relative
 if src.is_file() and not (DEST/relative).exists():
  (DEST/relative).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,DEST/relative);private_copies.append(relative)
local=DEST/'platformio.local.ini'
if not local.exists():local.write_text('; Isolated compatibility consumer; no hardware operations.\n')
# This profile changes only the throwaway consumer's dependency selection.
with local.open('a') as f:
 f.write('''\n[env:compat_async35_usb]\nextends = env:usb\nlib_deps =\n    ${env.lib_deps}\n    ESP32Async/AsyncTCP@3.5.0\nlib_ignore =\n    AsyncTCP-esphome\n''')
# Record identities and private-file names only, never their contents.
meta={'source_commit':subprocess.check_output(['git','-C',str(SOURCE),'rev-parse','main'],text=True).strip(),'consumer':str(DEST),'original_tracked_status':subprocess.check_output(['git','-C',str(SOURCE),'status','--porcelain'],text=True),'private_build_input_paths':private_copies,'cm_freeze':freeze,'candidate_dependency':'ESP32Async/AsyncTCP@3.5.0','ignored_dependency':'AsyncTCP-esphome','profiles':['usb','compat_async35_usb'],'product_version_changed':False}
(ROOT/'compat-consumer-inputs.json').write_text(json.dumps(meta,indent=2))
print('Created isolated archived main consumer; exact CM freeze copied unchanged.')
print('Build-local private input files copied without displaying contents:',len(private_copies))
print('Consumer:',DEST)
