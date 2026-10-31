# ESP32 Bluetooth Controller Candidate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the complete SBT with an official controller fix, run an extended isolated test, and retain the exact production rollback.

**Architecture:** Keep main and all SBT/CM runtime policies unchanged. Override only the linked controller archive inside this worktree with Espressif's IDF4.4.7-specific 0b40e99 release. Preserve 1.5.0 artifacts and private build inputs outside Git.

**Tech Stack:** PlatformIO espressif32 7.0.1, Arduino-ESP32 2.0.17 / IDF4.4.7-dirty, NimBLE-Arduino2.5.1, frozen CM4.10.5, Python build guard.

**Spec:** Owner instruction of 2026-10-31: build complete SBT, extended test on test ESP, then consider production; guarantee a path back to the current stand. This supersedes the proposed standalone BLE probe.

## Global Constraints

- Branch: fix/esp32-bt-controller; baseline main db714531b11ac0ad5ddeed09fd1de8f87f93f2cd.
- Firmware candidate1.5.1; no global SDK cache mutation, AsyncTCP upgrade, settings migration, MOS-policy change or Live activation.
- Candidate ZIP SHA256621c70f029f522328907a5815ae43107b19b85725cd3e46a7e07e4a9e535c63f; member4.4.7(0b40e99)/libbtdm_app.a SHA2563d890636991328a7a0aec387c876104372a1dabcd4920cec061268ecfe327b33.
- Test profile uses DHCP, SBTTest/COM8 topics, empty peers and blocked BMS control. Test-device identity must be confirmed before upload.
- Bench location has no BMS/Victron reachability: a passing bench test does not validate the real connection-state trigger.
- Production remains at1.5.0 until the test gate and recovery route are reviewed. A Git branch alone does not guarantee OTA recovery from an early boot failure.

## Review Focus

- Wrong, missing or modified controller archive: fail build before linking.
- Wrong chip/framework baseline: fail rather than silently linking an incompatible library.
- Linker ignores the override: fail post-link verification; inspect actual archive path and controller ID.
- Missing BMS radio coverage: explicitly qualify hardware-test results.
- Early controller fault on production: preserve exact old binary and require an OTA-reachable recovery window before the production experiment.

## Tasks

- [x] Create isolated worktree; preserve production ELF/bin/map, frozen CM and private inputs; create and verify source Git bundle.
- [x] Add failing tests for archive validation, SDK-baseline rejection and linked-archive provenance; all9pass after fixes.
- [x] Add an explicit, hash-pinned archive preparation command and a fail-closed post-framework build override.
- [x] Set1.5.1 and unique build directories; document official sources and rollback procedure.
- [x] Run guard tests and complete USB, OTA and isolated-test builds; prove actual linked controller identity and unchanged global SDK archive.
- [x] Review changes independently and verify test-ESP identity.
- [x] Complete300seconds with actual passive scan activity, then demonstrate fixture OTA rollback. The proposed6hours were explicitly superseded by the owner requesting production immediately.
- [x] Preserve production settings; install1.5.1 via ArduinoOTA with verified BLE-off recovery boot; enable BLE in RAM only.
- [x] Finish bounded real-device production observation and report limits. No main integration or branch deletion is part of this experiment.
