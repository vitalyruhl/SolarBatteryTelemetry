$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir=Join-Path $repo '.pio/host-tests/panic-summary'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
foreach($name in @('panic_summary_test','panic_summary_publisher_test','panic_summary_capture_test','panic_once_test','abort_details_test')) {
    $exe=Join-Path $outDir "$name.exe"
    & 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -Wall -Wextra -Werror -DSBT_ISOLATED_TEST_PROFILE=1 -I (Join-Path $repo 'test/host/panic_stubs') -I (Join-Path $repo 'src') (Join-Path $repo "test/host/$name.cpp") -o $exe
    if($LASTEXITCODE -ne 0){throw "$name host compile failed"}
    if($name -eq 'panic_summary_capture_test') {
        foreach($scenario in 0..11){& $exe $scenario;if($LASTEXITCODE -ne 0){throw "$name scenario $scenario failed"}}
    } else {& $exe;if($LASTEXITCODE -ne 0){throw "$name host assertions failed"}}
}
# A disabled probe must compile without Arduino; an enabled production probe must fail closed.
$guard=Join-Path $outDir 'panic_probe_guard.cpp'
[IO.File]::WriteAllText($guard, '#include "smoke/PanicOnceProbe.h"')
& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -I (Join-Path $repo 'src') -fsyntax-only $guard
if($LASTEXITCODE -ne 0){throw 'Disabled panic probe must compile without device dependencies'}
$rejection=& 'C:/Program Files/LLVM/bin/clang++.exe' -std=c++17 -DSBT_TEST_PANIC_ONCE=1 -I (Join-Path $repo 'src') -fsyntax-only $guard 2>&1
if($LASTEXITCODE -eq 0 -or ($rejection -join "`n") -notmatch 'Deliberate panic requires SBT_ISOLATED_TEST_PROFILE=1'){throw 'Production panic guard did not reject the unsafe profile'}
Write-Output '[MOCKED!] Compile-time production panic exclusion passed.'
# Inspect the actual application wiring, not only the portable boolean policy.
$main=[IO.File]::ReadAllText((Join-Path $repo 'src/main.cpp'))
$setupStart=$main.IndexOf('void setup() {')
$loopStart=$main.IndexOf('void loop() {', $setupStart)
if($setupStart -lt 0 -or $loopStart -lt 0){throw 'Application setup/loop boundary missing'}
$setup=$main.Substring($setupStart,$loopStart-$setupStart)
if($setup -notmatch '#if SBT_TEST_PANIC_ONCE\s+panicProbeSetupComplete=true;\s+#endif'){throw 'Actual successful setup must arm the isolated panic readiness flag'}
if($setup.IndexOf('panicProbeSetupComplete=true;') -lt $setup.IndexOf('ConfigManager.startWebServer();')){throw 'Panic readiness must follow successful setup completion'}
if($main -notmatch '#if SBT_TEST_PANIC_ONCE\s+static bool panicProbeSetupComplete=false;\s+#endif'){throw 'Panic readiness storage must be compiled out of normal profiles'}
if($main -notmatch 'tickPanicOnceProbe\(now,panicProbeSetupComplete&&bleWorkerReady&&telemetryMqtt.connected\(\)\)'){throw 'Actual loop must use the real setup completion flag'}
Write-Output '[MOCKED!] Actual setup completion assignment and isolated loop readiness wiring passed.'
exit 0
