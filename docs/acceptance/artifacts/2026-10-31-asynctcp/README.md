# Recovered AsyncTCP investigation, 2026-10-31

Recovered into SolarBatteryTelemetry on 2026-11-01 from
C:/Temp/asynctcp-contribution-20261031-152650/. This directory owns the research
artifacts previously referenced from the SBT acceptance record. No product
source, dependency declaration, firmware version, or hardware behavior changes.

## Historical evidence and provenance

historical/ preserves the 16 named original files byte-for-byte. Original
`<temporary-directory>/<name>` maps to historical/<name>;
historical/original-files.sha256 records every captured file hash. The old paths
and commands inside those captures describe the original experiment.
The Python scripts in historical/ are archival evidence, not supported entry
points: compatibility helpers depend on the former private consumer, ignored
CM freeze, local build inputs, and machine paths. Do not execute them as a
reconstruction procedure.

The original host record contains 18 expected observations: legacy unchecked
allocation faults and a queue-refill close deadlock are reproduced, while the
specified successor cases survive. The separate compatibility record reports
baseline, USB-candidate, and OTA-candidate compilation for SBT 1.4.12 / exact CM
4.10.5 on Arduino 2.0.17 / ESP-IDF 4.4.7. These are historical bounded results,
not proof of production crash causation or runtime stability. No source fix,
public contribution, hardware acceptance, or multi-day test follows from them.

Audited original checkouts were clean, shallow, and equal to these public refs:

| Upstream | Ref / commit | Use here |
| --- | --- | --- |
| [esphome/AsyncTCP](https://github.com/esphome/AsyncTCP) | main / 27cb883234bcea25cb952deefc571d97b3c09b0f | Legacy source input |
| [ESP32Async/AsyncTCP](https://github.com/ESP32Async/AsyncTCP) | v3.5.0 / 5bd9e2dbde8de5d0652e8e69565b8b561d866f18 | Released source and list header inputs |
| [ESP32Async/AsyncTCP](https://github.com/ESP32Async/AsyncTCP) | main / 02c90b8fb66bbcd55558cff6b1f81b395dc957f2 | Historical assessment only; not needed by the harness |

upstream/ contains only the two complete pinned src/AsyncTCP.cpp inputs, the
released src/AsyncTCPSimpleIntrusiveList.h, and each upstream LICENSE. They were
recovered unchanged from the audited working trees, retaining their copyright
notices and LGPL license. upstream/source-inputs.json records paths, commits,
SHA-256 hashes of exact captured bytes, and canonical source Git blobs.
Captured Windows line endings can differ from canonical Git blobs; historical
extraction-manifest.json's git_blob fields hash captured bytes instead.
.gitattributes prevents normalization of these original evidence/input bytes.
It also treats their captured CRLF endings as line endings during whitespace
checks, preserving the original bytes without disabling whitespace validation.
No full clone is retained here; upstream source is never part of firmware builds.

## Runnable offline host harness

host/ adapts only paths, input verification, and output handling from the
original harness tooling. Extracted upstream function bodies remain unchanged.
It checks every vendored input hash, regenerates both translation units, builds
them with an installed LLVM clang++, and runs the original 18 cases. All
FreeRTOS queues, TCP transport, application callbacks, and task scheduling are
explicit host models. This is not an ESP32/lwIP integration test.

From the repository root on Windows with Python 3 and LLVM installed:

~~~powershell
python docs/acceptance/artifacts/2026-10-31-asynctcp/host/run_host_tests.py
~~~

Use --compiler 'C:/Program Files/LLVM/bin/clang++.exe' if compiler discovery
needs an explicit path. No Git checkout, network access, firmware build, upload,
or device operation is performed. Generated source, executables, diagnostics,
and fresh test-results.json stay under ignored host/.build/; historical results
are never overwritten. Old OOM child-process access violations are expected
reproduced defects. The four old-source pointer-narrowing warnings on this
64-bit host are a known model/compiler limitation.

Recovery validation on 2026-11-01 with LLVM clang++ 22.1.8: both host builds
succeeded and all 18 specified observations matched. This rechecks the bounded
host model only; historical firmware compatibility builds were not rerun.

## Compatibility reconstruction limits

candidate-async35.ini.example retains only the non-secret USB and OTA candidate
sections from the temporary consumer. It does not change active project
configuration or authorize a migration. The original consumer was archived from
SBT commit 48726cad93ee822deac96c7631634cd38c28209b; its 122 tracked files
match that commit after CRLF normalization. The private ignored CM freeze and
build inputs remain necessary for exact historical firmware reconstruction.
Raw compatibility build logs, private configuration/credentials, full consumer,
firmware, binaries, PDB files, and temporary Git clones are deliberately absent.
The compatibility scripts remain historical; their original USB-only preparer
also omits the OTA section expected by the original runner.

## Ignored local recovery, 2026-11-01

Private recovery material is retained separately at these ignored repository paths.
It is not part of this tracked archive and must not be staged:

| Material | Ignored destination |
| --- | --- |
| AsyncTCP original root artifacts and private consumer inputs | .local/recovery/asynctcp-contribution-20261031-152650 |
| Cell-diagnostics local evidence | .local/recovery/sbt-cell-diagnostics-20261031 |
| Controller-fix local evidence | .local/recovery/sbt-controller-fix-20261031 |
| Controller extra run logs | .local/recovery/sbt-controller-fix-run-20261031 |
| Alternate controller recovery note | .local/recovery/sbt-controller-fix-recovery-20261031 |
| Restored controller library and package used by the existing build | .local/bt-controller/0b40e99 |

The AsyncTCP private copy includes the exact CM freeze and private configuration
and secret-header inputs. Their contents are absent from this archive.
These locations preserve local evidence; they are not active development checkouts.
