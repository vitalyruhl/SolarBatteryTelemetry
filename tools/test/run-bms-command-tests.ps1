$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output=Join-Path $repo '.pio/host-tests/bms_command_policy_test.exe'
New-Item -ItemType Directory -Force -Path (Split-Path $output) | Out-Null
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/bms_command_policy_test.cpp') -o $output
if($LASTEXITCODE -ne 0){throw 'BMS command host compile failed'}
& $output
if($LASTEXITCODE -ne 0){throw 'BMS command host checks failed'}

foreach($case in @('bms_command_envelope_test','status_publisher_test')) {
 $target=Join-Path $repo ('.pio/host-tests/'+$case+'.exe')
 & 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') -I (Join-Path $repo '.pio/ble-smoke/libdeps/usb/ArduinoJson/src') (Join-Path $repo ('test/host/'+$case+'.cpp')) -o $target
 if($LASTEXITCODE -ne 0){throw ($case+' compile failed')}
 & $target
 if($LASTEXITCODE -ne 0){throw ($case+' checks failed')}
}
