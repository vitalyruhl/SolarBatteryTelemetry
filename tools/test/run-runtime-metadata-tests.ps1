param([string]$ConfigManagerPath = 'C:/Daten/_Codding/ESP32/ConfigurationsManager')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$cm = (Resolve-Path -LiteralPath $ConfigManagerPath).Path
$outDir = Join-Path $repo '.pio/host-tests'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir 'runtime_metadata_test.exe'
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') -I (Join-Path $cm 'src') (Join-Path $repo 'test/host/runtime_metadata_test.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Compact runtime metadata host compile failed' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Compact runtime metadata assertions failed' }
