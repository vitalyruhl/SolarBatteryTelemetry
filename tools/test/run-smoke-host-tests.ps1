$ErrorActionPreference = 'Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir=Join-Path $repo '.pio/host-tests'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/smoke_policy_test.cpp') -o (Join-Path $outDir 'smoke.exe')
if($LASTEXITCODE -ne 0){throw 'Smoke host compile failed'}
& (Join-Path $outDir 'smoke.exe')
if($LASTEXITCODE -ne 0){throw 'Smoke provisioning assertions failed'}
& python -m unittest discover -s (Join-Path $repo 'test/host') -p test_precompile.py
if($LASTEXITCODE -ne 0){throw 'ConfigManager dependency validation tests failed'}
