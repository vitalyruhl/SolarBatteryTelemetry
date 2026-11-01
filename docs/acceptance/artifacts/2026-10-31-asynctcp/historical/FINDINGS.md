# AsyncTCP historical fault reproduction and upstream assessment

Recorded: 2026-10-31. Scope: public AsyncTCP source and deterministic host tests only.

## Decision

Both suspected failure modes are reproducible in AsyncTCP-esphome 2.1.4. The released ESP32Async/AsyncTCP v3.5.0 already handles the tested allocation failures and avoids the tested bounded-queue self-deadlock. A new public fork, fix, or duplicate bug report for these two historical faults is not justified by this evidence. A separately isolated compatibility assessment of the maintained successor is the useful next step.

This does not establish the cause of any real-device panic or MQTT failure. No production device or live UI was operated. No multi-day hardware test was performed. No public fork, issue, comment, commit, push, or PR was created.

## Upstream and contribution route

- [esphome/AsyncTCP](https://github.com/esphome/AsyncTCP) is public and not archived, but its issue tracker is disabled. Main is `27cb883234bcea25cb952deefc571d97b3c09b0f`; the latest push was 2026-04-13. Its README describes an ESPHome fork. It does not claim that it has moved.
- [me-no-dev/AsyncTCP README](https://github.com/me-no-dev/AsyncTCP/blob/master/README.md) explicitly points to [ESP32Async/AsyncTCP](https://github.com/ESP32Async/AsyncTCP) as the successor.
- ESP32Async/AsyncTCP is public, not archived, and accepts bug reports. [v3.5.0](https://github.com/ESP32Async/AsyncTCP/releases/tag/v3.5.0), commit `5bd9e2dbde8de5d0652e8e69565b8b561d866f18`, is the latest published GitHub release observed. Its current main is `02c90b8fb66bbcd55558cff6b1f81b395dc957f2`, whose authored version is 4.0.0; this is not the same as the released version.
- Both source trees contain the GNU Lesser General Public License version 3. The repository metadata identifies `LGPL-3.0`.
- No `AGENTS.md`, `AGENTS.override.md`, or `CONTRIBUTING.md` was found in either recursively enumerated repository tree. An organization-level `CONTRIBUTING.md` request returned 404 for both organizations. This is not a comprehensive claim about every organization policy.
- [ESP32Async's bug form](https://github.com/ESP32Async/AsyncTCP/blob/02c90b8fb66bbcd55558cff6b1f81b395dc957f2/.github/ISSUE_TEMPLATE/issue-report.yml) requires the latest version, an MRE, and searches of issues, discussions, documentation, and examples. We do not have a current-version reproduction of either historical fault, so there is no basis for filing such a report now.
- External code contributions are accepted in practice: [PR #124](https://github.com/ESP32Async/AsyncTCP/pull/124), submitted by `willmmiles`, was merged on 2026-10-25. Its author explicitly describes AI-assisted test development. This supports contribution acceptance; it is not a blanket acceptance guarantee for a future patch.
- [Code of Conduct](https://github.com/ESP32Async/AsyncTCP/blob/02c90b8fb66bbcd55558cff6b1f81b395dc957f2/CODE_OF_CONDUCT.md) applies to community participation.

## Bounded duplicate and related-work search

GitHub connector searches covered `deadlock` and `malloc` in each repository, plus `queue` and `allocation` among ESP32Async PRs. This was a bounded search, not an exhaustive search of every historical discussion.

- [ESPHome PR #17](https://github.com/esphome/AsyncTCP/pull/17), proposing a move to ESP32Async, is closed and unmerged. It must not be cited as evidence of a completed migration.
- [ESP32Async PR #21](https://github.com/ESP32Async/AsyncTCP/pull/21), proposing a bounded-queue replacement, is also closed and unmerged. The released implementation itself, rather than this PR's status, establishes that the queue architecture changed.
- [PR #118](https://github.com/ESP32Async/AsyncTCP/pull/118) concerns a different abort-path use-after-free and is listed in the v3.5.0 release notes.
- [Issue #128](https://github.com/ESP32Async/AsyncTCP/issues/128) concerns a different pre-ACCEPT reset leak. [PR #124](https://github.com/ESP32Async/AsyncTCP/pull/124) describes broader lifetime, DNS, callback, and server fixes on current main. Passing this report's tests does not establish that v3.5.0 is free of those other issues.
- [PR #129](https://github.com/ESP32Async/AsyncTCP/pull/129), a regression test harness, is open. Any future contribution should coordinate with that effort instead of duplicating it.

## Source identity and test method

The installed 2.1.4 `AsyncTCP.cpp` has Git blob `b1d5e37f66316739db7c38c4e1641b8c6a438b3e`, identical to public ESPHome main. The reproduction contains only public library source, neutral test stubs, and authored harness code. No proprietary application source, device configuration, credential, private address, or production log is included.

`prepare_harness.py` extracts complete unchanged definitions from pinned checkout source into two host translation units. Verification confirmed all 16 selected old definitions and all 17 selected stable definitions occur unchanged in the generated harnesses. `extraction-manifest.json` records checkout commits, source SHA-256 values, Git blobs, and selected symbols. All three public upstream checkouts remain clean.

Compiler: LLVM clang++ 22.1.8, x86_64 Windows target, C++17, `-O0 -g`. Both builds succeeded. The old build emits four pointer-to-int narrowing warnings in the unchanged `_remove_events_with_arg` function because the host has 64-bit pointers; this warning is not used as evidence for either tested defect. The stable build emits no warning.

Platform boundaries are explicit host stubs: FreeRTOS's bounded queue is modeled by a capacity-32 queue with real C++ mutex/condition-variable waits; semaphore operations use a C++ mutex; TCP transport operations and the synchronous `tcpip_api_call` handoff are stubs; user ACK callback entry is a stub. The old actual event dispatcher, `close`, `_close`, `_tcp_clear_events`, and queue-send helpers execute unchanged. The stable actual dispatcher, `close`, `_close`, TCP close helper/API function, callback reset, event removal, and upstream intrusive-list header execute unchanged. This is an actual-source unit harness, not an ESP32 FreeRTOS or lwIP integration test.

## Results

All 18 cases produced their specified observations in `test-results.json`.

| Case | AsyncTCP-esphome 2.1.4 | ESP32Async v3.5.0 |
| --- | --- | --- |
| Allocation failure: poll, receive, sent, connected | Four access violations, `0xC0000005` | Four `ERR_MEM` returns, empty queue |
| Allocation failure: error and DNS | Two access violations, `0xC0000005` | Two clean returns, empty queue |
| Allocation failure: clear and accept | Two access violations, `0xC0000005` | These old helper APIs are absent; not claimed as directly comparable tests |
| ACK callback closes client; producer does not refill | Callback completes; queue count 32 | Callback completes; queue count 31 |
| ACK callback closes client; producer refills freed slot first | Full queue, sole consumer blocks in enqueue, callback incomplete | Callback completes; queue count 32 |

### Allocation failure mechanism

The old callbacks use an unchecked `malloc` result and immediately write an event field. Returning null from the allocator causes the access violation before queue submission. Each callback is tested in a separate process. The released successor uses `new (std::nothrow)`, tests the result before dereferencing it, and returns `ERR_MEM` or returns from a void callback. The six common callbacks survive the same injected allocation failure.

### Queue self-deadlock mechanism

1. Fill 32 events; the first is an ACK event for the target client and the remaining 31 belong to another client.
2. The sole modeled service consumer dequeues that ACK and reaches its application callback.
3. Pause the callback, enqueue one other-client event to refill the free slot, then permit the callback to close the target client.
4. The old `close` path prepends a CLEAR event to the same full queue with `portMAX_DELAY`. The only consumer is inside that enqueue and cannot make space. The harness observes the blocking wait and incomplete callback before explicitly releasing its host queue for orderly test shutdown.
5. The successor closes through direct removal of matching client events from an intrusive list and returns under the same scheduled interleaving. No bounded enqueue is needed by this path.

The no-refill controls establish that a full queue during callback close is the decisive condition. The harness does not execute a task watchdog or demonstrate that the observed production reboot took this path.

## Reproduction and retained artifacts

Use the task directory as the current directory and run the following PowerShell commands:

```powershell
python prepare_harness.py
& 'C:\Program Files\LLVM\bin\clang++.exe' -std=c++17 -O0 -g .\old_harness.cpp -o .\old_harness.exe
& 'C:\Program Files\LLVM\bin\clang++.exe' -std=c++17 -O0 -g .\stable_harness.cpp -o .\stable_harness.exe
python run_harness.py
```
 The runner intentionally expects access violations in old-version OOM child processes; these are recorded as reproduced defects, not as healthy application behavior.

Retained files: `prepare_harness.py`, `run_harness.py`, `old_harness.cpp`, `stable_harness.cpp`, `old-build.log`, `stable-build.log`, `extraction-manifest.json`, and `test-results.json`. The pinned public checkouts and local test executables are retained for review and reproduction.

## Remaining gate

A compatibility migration must first resolve the application APIs and dependency graph against a deliberately pinned successor version. Neither the released v3.5.0 nor unreleased main should be promoted to production on this host-test evidence alone. Any new patch or eventual PR remains conditional on a current-source defect, reviewable scope, and the requested successful multi-day hardware test. That test has not started here.
