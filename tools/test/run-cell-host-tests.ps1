$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output=Join-Path $repo '.pio/host-tests/cells.exe'
New-Item -ItemType Directory -Force -Path (Split-Path $output) | Out-Null
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/cell_publisher_test.cpp') -o $output
if($LASTEXITCODE -ne 0){throw 'Cell host compile failed'}
& $output
if($LASTEXITCODE -ne 0){throw 'Cell host checks failed'}
