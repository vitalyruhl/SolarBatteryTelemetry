$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$outDir=Join-Path $repo '.local/heap-diagnostics'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$compiler='C:/Program Files/LLVM/bin/clang++.exe'
$nm='C:/Program Files/LLVM/bin/llvm-nm.exe'
$common=@('-std=c++17','-Wall','-Wextra','-Werror','-DESP32','-I',(Join-Path $repo 'test/host/heap_failure_stubs'),'-I',(Join-Path $repo 'src'))
$probe=Join-Path $repo 'src/diagnostics/ThreadSemaphoreDiagnostics.cpp'
$runtime=Join-Path $repo 'test/host/heap_failure_stubs/task_runtime.cpp'
foreach ($profile in @(@(0,0),@(0,1),@(1,0))) {
 $verbose=$profile[0]; $diagnostics=$profile[1]
 $exe=Join-Path $outDir "heap-failure-$verbose-$diagnostics.exe"
 & $compiler @common "-DCM_ENABLE_VERBOSE_LOGGING=$verbose" "-DSBT_ENABLE_HEAP_DIAGNOSTICS=$diagnostics" (Join-Path $repo 'test/host/heap_failure_diagnostics_test.cpp') $probe $runtime -o $exe
 if($LASTEXITCODE -ne 0){throw "Heap failure compile failed (verbose=$verbose, diagnostics=$diagnostics)."}
 & $exe
 if($LASTEXITCODE -ne 0){throw "Heap failure test failed (verbose=$verbose, diagnostics=$diagnostics)."}
}
$exe=Join-Path $outDir 'thread-semaphore.exe'
& $compiler @common -DCM_ENABLE_VERBOSE_LOGGING=0 -DSBT_ENABLE_HEAP_DIAGNOSTICS=1 (Join-Path $repo 'test/host/thread_semaphore_diagnostics_test.cpp') $probe $runtime -o $exe
if($LASTEXITCODE -ne 0){throw 'Thread semaphore compile failed.'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Thread semaphore tests failed.'}
foreach ($diagnostics in @(0,1)) {
 $object=Join-Path $outDir "thread-semaphore-$diagnostics.obj"
 & $compiler @common -DCM_ENABLE_VERBOSE_LOGGING=0 "-DSBT_ENABLE_HEAP_DIAGNOSTICS=$diagnostics" -c $probe -o $object
 if($LASTEXITCODE -ne 0){throw 'Diagnostic object compile failed.'}
 $undefined=(& $nm --undefined-only $object | Out-String)
 if($LASTEXITCODE -ne 0){throw 'Diagnostic undefined-symbol inspection failed.'}
 if($undefined -match '(malloc|calloc|realloc|operator new|\?\?2)'){throw 'Diagnostic probe references an allocator.'}
 $defined=(& $nm --defined-only $object | Out-String)
 if($LASTEXITCODE -ne 0){throw 'Diagnostic defined-symbol inspection failed.'}
 if($diagnostics -eq 0 -and $defined -match '(wrap_sys_thread_sem_get|recordAllocationFailure|history|failureCount|dropped)') {throw 'Disabled diagnostic probe retained code or storage.'}
 if($diagnostics -eq 1 -and $defined -notmatch '__wrap_sys_thread_sem_get') {throw 'Enabled diagnostic wrapper is missing.'}
}
Write-Output 'Diagnostic object checks passed: no allocator references and disabled probe compiled out.'
