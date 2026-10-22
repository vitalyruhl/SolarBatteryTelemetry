$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir=Join-Path $repo '.pio/host-tests/health-age'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
# Compile the production functions, replacing only their external boundaries in the host fixture.
$main=[IO.File]::ReadAllText((Join-Path $repo 'src/main.cpp'))
$start=$main.IndexOf('void loop() {')
$end=$main.IndexOf('void onWiFiConnected()', $start)
if($start -lt 0 -or $end -lt 0){throw 'Production loop boundaries not found'}
[IO.File]::WriteAllText((Join-Path $outDir 'production_loop.inc'),$main.Substring($start,$end-$start))
$service=[IO.File]::ReadAllText((Join-Path $repo 'src/telemetry/MqttService.h'))
$start=$service.IndexOf(' void health(')
$end=$service.IndexOf(' void controlStatus()', $start)
if($start -lt 0 -or $end -lt 0){throw 'Production source health boundaries not found'}
[IO.File]::WriteAllText((Join-Path $outDir 'production_health.inc'),$service.Substring($start,$end-$start))
$exe=Join-Path $outDir 'health_age_test.exe'
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -I $outDir (Join-Path $repo 'test/host/health_age_test.cpp') -o $exe
if($LASTEXITCODE -ne 0){throw 'Health age host compile failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Health age regression failed'}
