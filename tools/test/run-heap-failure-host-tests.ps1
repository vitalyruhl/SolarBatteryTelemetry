$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir=Join-Path $repo '.local/heap-diagnostics'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
foreach ($verbose in @(0,1)) {
 $exe=Join-Path $outDir "heap-failure-$verbose.exe"
 & 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -DESP32 "-DCM_ENABLE_VERBOSE_LOGGING=$verbose" -I (Join-Path $repo 'test/host/heap_failure_stubs') -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/heap_failure_diagnostics_test.cpp') -o $exe
 if($LASTEXITCODE -ne 0){throw "Heap failure compile failed (verbose=$verbose)."}
 & $exe
 if($LASTEXITCODE -ne 0){throw "Heap failure test failed (verbose=$verbose)."}
}
