param([string]$ConfigManagerPath = 'C:/Daten/_Codding/ESP32/ConfigurationsManager', [string]$ArduinoJsonInclude = '')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$cm = (Resolve-Path -LiteralPath $ConfigManagerPath).Path
if (!$ArduinoJsonInclude) {
    $candidates = @('.pio/ble-smoke/libdeps/usb/ArduinoJson/src', '.pio/ble-smoke/libdeps/heap-diagnostics/ArduinoJson/src')
    $ArduinoJsonInclude = $candidates | ForEach-Object { Join-Path $repo $_ } | Where-Object { Test-Path (Join-Path $_ 'ArduinoJson.h') } | Select-Object -First 1
}
if (!$ArduinoJsonInclude -or !(Test-Path (Join-Path $ArduinoJsonInclude 'ArduinoJson.h'))) { throw 'Build usb first or supply ArduinoJsonInclude.' }
$outDir = Join-Path $repo '.pio/host-tests'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir 'runtime_metadata_test.exe'
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') -I (Join-Path $cm 'src') -I $ArduinoJsonInclude (Join-Path $repo 'test/host/runtime_metadata_test.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Compact runtime metadata host compile failed' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Compact runtime metadata assertions failed' }
