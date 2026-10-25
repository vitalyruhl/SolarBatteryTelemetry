param([string]$ConfigManagerPath = '', [string]$ArduinoJsonInclude = '')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$ConfigManagerPath) { $ConfigManagerPath = Join-Path $repo '.pio/registry-settings/libdeps/usb/ESP32 Configuration Manager' }
$cm = (Resolve-Path -LiteralPath $ConfigManagerPath).Path
$manifest = Get-Content -LiteralPath (Join-Path $cm 'library.json') -Raw | ConvertFrom-Json
if ($manifest.version -ne '4.9.0') { throw 'Runtime host tests require published Registry CM 4.9.0.' }
if (!$ArduinoJsonInclude) {
    $candidates = @('.pio/registry-settings/libdeps/usb/ArduinoJson/src', '.pio/registry-settings/libdeps/testesp/ArduinoJson/src')
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
