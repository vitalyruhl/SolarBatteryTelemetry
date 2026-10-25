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

foreach($case in @('telemetry_contract_test','victron_history_test','mqtt_mailbox_test','mqtt_lifecycle_test')) {
    $exe=Join-Path $outDir ($case+'.exe')
    & 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -I (Join-Path $repo 'src') (Join-Path $repo ('test/host/'+$case+'.cpp')) -o $exe
    if($LASTEXITCODE -ne 0){throw ($case+' compile failed')}
    & $exe
    if($LASTEXITCODE -ne 0){throw ($case+' assertions failed')}
}
foreach($profile in @(0,1)) {
    $exe=Join-Path $outDir ('isolated_namespace_'+$profile+'.exe')
    & 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror "-DSBT_ISOLATED_TEST_PROFILE=$profile" -I (Join-Path $repo 'src') (Join-Path $repo 'test/host/isolated_namespace_test.cpp') -o $exe
    if($LASTEXITCODE -ne 0){throw 'Isolated namespace host compile failed'}
    & $exe
    if($LASTEXITCODE -ne 0){throw 'Isolated namespace assertions failed'}
}
