param([string]$ConfigManagerPath = '', [string]$ArduinoJsonInclude = '')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$ConfigManagerPath) {
    $ConfigManagerPath = @('ota','testesp','usb') | ForEach-Object { Join-Path $repo ".pio/wifi-coex-1.4.8/libdeps/$_/ESP32 Configuration Manager" } | Where-Object { Test-Path (Join-Path $_ 'library.json') } | Select-Object -First 1
}
$cm = (Resolve-Path -LiteralPath $ConfigManagerPath).Path
$manifest = Get-Content -LiteralPath (Join-Path $cm 'library.json') -Raw | ConvertFrom-Json
if ($manifest.version -notin @('4.9.0','4.10.1')) { throw 'Runtime host tests require published CM 4.9.0 or frozen CM 4.10.1.' }
if ($manifest.version -eq '4.10.1') {
    & python -c 'import sys; from pathlib import Path; sys.dont_write_bytecode=True; sys.path.insert(0,sys.argv[1]); from prepare_cm_development import verify_development_package; verify_development_package(Path(sys.argv[2]),Path(sys.argv[3]))' (Join-Path $repo 'tools') (Join-Path $repo '.local/cm-wifi-coex-4.10.1-package') $cm
    if ($LASTEXITCODE -ne 0) { throw 'Frozen CM 4.10.1 source verification failed.' }
}
if (!$ArduinoJsonInclude) {
    $candidates = @('.pio/wifi-coex-1.4.8/libdeps/ota/ArduinoJson/src', '.pio/wifi-coex-1.4.8/libdeps/testesp/ArduinoJson/src', '.pio/registry-settings/libdeps/usb/ArduinoJson/src')
    $ArduinoJsonInclude = $candidates | ForEach-Object { Join-Path $repo $_ } | Where-Object { Test-Path (Join-Path $_ 'ArduinoJson.h') } | Select-Object -First 1
}
if (!$ArduinoJsonInclude -or !(Test-Path (Join-Path $ArduinoJsonInclude 'ArduinoJson.h'))) { throw 'Build usb first or supply ArduinoJsonInclude.' }
$outDir = Join-Path $repo '.pio/host-tests'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir 'runtime_metadata_test.exe'
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -DCM_ENABLE_LIVE_UI=1 -I (Join-Path $repo 'src') -I (Join-Path $cm 'src') -I $ArduinoJsonInclude (Join-Path $repo 'test/host/runtime_metadata_test.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Compact runtime metadata host compile failed' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Compact runtime metadata assertions failed' }
$disabled = Join-Path $outDir 'runtime_metadata_disabled_test.exe'
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -DCM_ENABLE_LIVE_UI=0 -I (Join-Path $repo 'src') -I (Join-Path $cm 'src') -I $ArduinoJsonInclude (Join-Path $repo 'test/host/runtime_metadata_disabled_test.cpp') -o $disabled
if ($LASTEXITCODE -ne 0) { throw 'Settings-only runtime metadata host compile failed' }
& $disabled
if ($LASTEXITCODE -ne 0) { throw 'Settings-only startup assertions failed' }
