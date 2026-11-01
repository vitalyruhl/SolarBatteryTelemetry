from pathlib import Path
import hashlib,json,re,subprocess
r=Path(__file__).resolve().parent;c=r/'sbt-compat-consumer';source=Path(r'C:\Daten\_Codding\ESP32\SolarBatteryTelemetry')
s=json.loads((r/'compat-build-status.json').read_text())
assert s['state']=='succeeded',s
rows=[]
for stage in s['stages']:
 env=stage['environment'];log=Path(stage['log']).read_text(errors='replace');lines=log.splitlines()
 ram=re.search(r'RAM:.*used (\d+) bytes from (\d+) bytes',log);flash=re.search(r'Flash:.*used (\d+) bytes from (\d+) bytes',log)
 assert stage['returncode']==0 and '[SUCCESS]' in log and ram and flash
 new=bool(re.search(r'Compiling.*[\\/]AsyncTCP[\\/]AsyncTCP.cpp.o',log));old=bool(re.search(r'Compiling.*AsyncTCP-esphome',log))
 if env!='usb':assert new and not old
 row={**stage,'ram_bytes':int(ram[1]),'flash_bytes':int(flash[1]),'warnings':sum('warning:' in x for x in lines),'new_tcp_compiled':new,'old_tcp_compiled':old,'cm_units':sum('Compiling' in x and 'ESP32 Configuration Manager' in x for x in lines),'webserver_units':sum('Compiling' in x and 'ESPAsyncWebServer' in x for x in lines)}
 assert row['cm_units']==21 and row['webserver_units']==7
 rows.append(row)
def hashes(p):return {x.relative_to(p).as_posix():hashlib.sha256(x.read_bytes()).hexdigest() for x in p.rglob('*') if x.is_file()}
freeze='.local/cm-ble-reboot-4.10.5-final-package';original=hashes(source/freeze);assert original==hashes(c/freeze)
assert not subprocess.check_output(['git','-C',str(source),'status','--porcelain'],text=True)
head=subprocess.check_output(['git','-C',str(source),'rev-parse','main'],text=True).strip();assert head=='48726cad93ee822deac96c7631634cd38c28209b'
def warnings(env):return {re.sub(r'^.*?warning:','warning:',l) for l in (r/('compat-build-'+env+'.log')).read_text(errors='replace').splitlines() if 'warning:' in l}
new_warnings=sorted(warnings('compat_async35_usb')-warnings('usb'))
norm=lambda p:p.read_bytes().replace(b'\r\n',b'\n')
for env in ['compat_async35_usb','compat_async35_ota']:
 assert norm(c/'.pio/ble-reboot-1.4.12/libdeps'/env/'AsyncTCP/src/AsyncTCP.cpp')==norm(r/'esp32async-stable/src/AsyncTCP.cpp')
matrix={'SBT':'1.4.12','CM':'4.10.5 exact unchanged freeze','ESPAsyncWebServer-esphome':'3.4.0','Baseline AsyncTCP-esphome':'2.1.4','Candidate ESP32Async/AsyncTCP':'3.5.0','PlatformIO espressif32':'7.0.1','Arduino ESP32':'2.0.17 (3.20017.241212+sha.dcc1105b)','ESP-IDF':'4.4.7','Xtensa toolchain':'8.4.0+2021r2-patch5','NimBLE-Arduino':'2.5.1','PubSubClient':'2.8.0','ArduinoJson':'7.4.3'}
summary={'source_commit':head,'source_clean':True,'unchanged_freeze_files':len(original),'live_ui':0,'settings_ui':1,'matrix':matrix,'builds':rows,'new_warning_messages':new_warnings,'scope':'Build compatibility only; no uploads, hardware/runtime tests, multi-day test or public mutation.'}
(r/'COMPATIBILITY-RESULTS.json').write_text(json.dumps(summary,indent=2))
text=['# Isolated SBT / AsyncTCP compatibility build','','Private local evidence. Do not publish the consumer, firmware, ignored inputs or raw logs.','','USB and OTA-profile compilation succeed with released AsyncTCP 3.5.0 and the exact CM freeze on Arduino 2.0.17 / ESP-IDF 4.4.7. The unchanged baseline succeeds. No source API patch or SDK upgrade was needed.','','## Version matrix','','| Component | Version |','| --- | --- |']
text += ['| '+k+' | '+v+' |' for k,v in matrix.items()]
text += ['','Board nodemcu-32s; partition dual-ota-1792k.csv; C++17 and LTO retained. All stages confirmed WebUI Live0 Settings1 and identical UI asset hashes.','','## Builds','','| Environment | Result | RAM / 327680 | Flash / 1835008 | Warnings | Seconds |','| --- | --- | --- | --- | --- | --- |']
text += ['| '+v['environment']+' | PASS | '+str(v['ram_bytes'])+' | '+str(v['flash_bytes'])+' | '+str(v['warnings'])+' | '+str(v['elapsed_seconds'])+' |' for v in rows]
text += ['','Each build compiles 21 CM and 7 ESPAsyncWebServer units. Candidates compile only successor AsyncTCP, never AsyncTCP-esphome. Registry source matches official v3.5.0 after newline normalization. Candidate USB static RAM is 72 bytes smaller and flash 4 bytes larger; these are linker estimates, not runtime heap measurements.','','## Alias and integrity checks','','CM and WebServer metadata still request the old package. Only the temporary consumer profile adds exact ESP32Async/AsyncTCP@3.5.0 and lib_ignore = AsyncTCP-esphome. The old transitive package is downloaded but excluded from compilation/linking. WebServer 3.4.0 and CM remain compiled. This experiment does not adopt a product dependency policy.','','The consumer is a git archive of main commit '+head+'. All '+str(len(original))+' freeze files match their originals by SHA-256 after building. Owning source remains clean. Two ignored inputs were copied without printing contents and remain private. No owning CM/SBT/SIL files or feature worktrees changed. Version 1.4.12 remains for this disposable experiment.','','New distinct warning messages compared with baseline:','']
text += ['- '+w for w in new_warnings]
text += ['','## Evidence and limits','','Compile-only PlatformIO environments: usb, compat_async35_usb and compat_async35_ota, stopping on any failure. No upload, serial monitor, device operation or live UI access. Files: compat-build-status.json, three compat-build-*.log files, compat-consumer-inputs.json, COMPATIBILITY-RESULTS.json, and consumer platformio.local.ini.','','This proves build compatibility only. No runtime/HIL or multi-day stability test; production panic and MQTT failure causes remain unproven. Product migration needs a reviewable dependency change and requested multi-day acceptance. The v3.5.0 release notes describe changed abort-callback task context; compilation does not validate application assumptions about that behavior. Unreleased main 4.0.0 was not substituted for this release.']
(r/'COMPATIBILITY-REPORT.md').write_text('\n'.join(text)+'\n')
print(json.dumps({'verified':True,'builds':rows,'new_warning_messages':new_warnings,'unchanged_freeze_files':len(original)},indent=2))
print('Report:',r/'COMPATIBILITY-REPORT.md')
