"""Focused portability contracts; synthetic package/environment data is [MOCKED!]."""

import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import prepare_cm_development as frozen
import cm_registry
import c3_cm_portability


ANCHOR = b'std::min(9999u, static_cast<uint32_t>(minutes))'
PATCHED = b'std::min<uint32_t>(9999u, static_cast<uint32_t>(minutes))'
NAMES = ('src/ConfigManager.h', 'src/core/CoreSettings.h')
HELPERS = ('src/helpers/HelperModule.h', 'src/helpers/HelperModule.cpp')
ALL_DERIVED = (*NAMES, *HELPERS)

# [MOCKED!] Minimal source fixtures include the original helper's exact patch sites.
HELPER_HEADER = b'''#include <Arduino.h>

class PulseOutput {
  PulseOutput(uint8_t pin, ActiveLevel level = ActiveLevel::ActiveHigh);
private:
  void writeLevel(bool onLogical);
  bool activeHigh_;
};
'''
HELPER_CPP = b'''PulseOutput::PulseOutput(uint8_t pin, ActiveLevel level)
    : pin_(pin), activeHigh_(level == ActiveLevel::ActiveHigh) {
  pinMode(pin_, OUTPUT);
  writeLevel(false);
  // register instance
}
void PulseOutput::loop() {
  const unsigned long now = millis();
}
void PulseOutput::writeLevel(bool onLogical) {
  const bool level = activeHigh_ ? onLogical : !onLogical;
}
'''


class MockedEnvironment:
    """[MOCKED!] Minimal PlatformIO environment for integration checks."""

    def __init__(self, project, mcu):
        self.project, self.mcu = project, mcu

    def subst(self, value):
        return {'$PROJECT_DIR': str(self.project), '$PROJECT_LIBDEPS_DIR': str(self.project / 'libdeps'),
                '$PIOENV': 'mocked'}[value]

    def BoardConfig(self):
        return {'build.mcu': self.mcu}


class PortabilityTests(unittest.TestCase):
    def setUp(self):
        self.base = Path(__file__).resolve().parents[2] / '.pio'
        self.base.mkdir(exist_ok=True)
        self.root = Path(tempfile.mkdtemp(prefix='test-cm-c3-', dir=self.base))
        self.package = self.root / cm_registry.DEVELOPMENT_PIN.removeprefix('file://')
        self.consumer = self.root / 'libdeps/mocked/ESP32 Configuration Manager'
        self.package.mkdir(parents=True)
        data = {
            'library.json': json.dumps({'name': 'ESP32 Configuration Manager', 'version': '4.10.5'}).encode(),
            NAMES[0]: b'#define CONFIGMANAGER_VERSION "4.10.5"\n// [MOCKED!]\n' + ANCHOR + b';\n',
            NAMES[1]: b'// [MOCKED!]\n' + ANCHOR + b';\n',
            HELPERS[0]: HELPER_HEADER,
            HELPERS[1]: HELPER_CPP,
            'webui/package.json': b'{"version":"4.10.5"}',
            'tools/unchanged.py': b'# [MOCKED!] unrelated immutable source\n',
        }
        for name, content in data.items():
            path = self.package / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        self.refresh_manifest()
        shutil.copytree(self.package, self.consumer)
        (self.consumer / '.piopm').write_text(json.dumps({
            'version': '4.10.5', 'spec': {'uri': cm_registry.DEVELOPMENT_PIN}}))

    def refresh_manifest(self):
        entries = sorted(p for p in self.package.rglob('*') if p.is_file() and p.name != frozen.MANIFEST)
        lines = ['# source_version=4.10.5'] + [
            hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.relative_to(self.package).as_posix()
            for p in entries]
        (self.package / frozen.MANIFEST).write_text('\n'.join(lines) + '\n')

    def tearDown(self):
        target = self.root.resolve()
        self.assertEqual(target.parent, self.base.resolve())
        self.assertTrue(target.name.startswith('test-cm-c3-'))
        shutil.rmtree(target)

    def run_target(self, mcu='esp32c3'):
        return cm_registry.development_package(MockedEnvironment(self.root, mcu))

    def test_c3_generates_only_four_files_and_is_idempotent(self):
        before = {p.relative_to(self.package): p.read_bytes() for p in self.package.rglob('*') if p.is_file()}
        self.assertEqual(self.run_target(), self.consumer)
        for name in NAMES:
            self.assertEqual((self.consumer / name).read_bytes(), before[Path(name)].replace(ANCHOR, PATCHED))
        after = {p.relative_to(self.consumer): p.read_bytes() for p in self.consumer.rglob('*') if p.is_file()}
        self.run_target()
        self.assertEqual(after, {p.relative_to(self.consumer): p.read_bytes() for p in self.consumer.rglob('*') if p.is_file()})
        self.assertEqual(before, {p.relative_to(self.package): p.read_bytes() for p in self.package.rglob('*') if p.is_file()})
        changed = {str(p).replace('\\', '/') for p in before if before[p] != (self.consumer / p).read_bytes()}
        self.assertEqual(changed, set(ALL_DERIVED))
        self.assertIn(b'  void initialize();', (self.consumer / HELPERS[0]).read_bytes())
        self.assertIn(b'  initialize();', (self.consumer / HELPERS[1]).read_bytes())

    def test_classic_and_other_targets_remain_strict(self):
        for mcu in ('esp32', 'esp32s3', 'ESP32C3', None):
            self.run_target(mcu)
        self.run_target()
        with self.assertRaisesRegex(RuntimeError, 'stale'):
            self.run_target('esp32')
        with self.assertRaisesRegex(RuntimeError, 'stale'):
            frozen.verify_development_package(self.package, self.consumer)

    def test_rejects_dirty_consumer_before_any_patch(self):
        for name in (*ALL_DERIVED, 'tools/unchanged.py'):
            with self.subTest(name=name):
                path = self.consumer / name
                original = path.read_bytes()
                path.write_bytes(original + b'// unexpected\n')
                with self.assertRaises(RuntimeError):
                    self.run_target()
                path.write_bytes(original)
        self.assertNotIn(PATCHED, (self.consumer / NAMES[0]).read_bytes())

    def test_rejects_extra_consumer_file(self):
        (self.consumer / 'src/unexpected.h').write_bytes(b'// [MOCKED!]')
        with self.assertRaisesRegex(RuntimeError, 'tree differs'):
            self.run_target()

    def test_accepts_partially_derived_consumer_after_interrupted_generation(self):
        derived = c3_cm_portability.expected_content(self.package)
        for subset in ((NAMES[0],), NAMES, HELPERS, ALL_DERIVED[::2]):
            with self.subTest(subset=subset):
                for name in ALL_DERIVED:
                    content = derived[name] if name in subset else (self.package / name).read_bytes()
                    (self.consumer / name).write_bytes(content)
                self.run_target()
                for name in ALL_DERIVED:
                    self.assertEqual((self.consumer / name).read_bytes(), derived[name])

    def test_classic_rejects_helper_only_derivations(self):
        self.run_target()
        for name in NAMES:
            (self.consumer / name).write_bytes((self.package / name).read_bytes())
        with self.assertRaisesRegex(RuntimeError, 'stale'):
            self.run_target('esp32')

    def test_rejects_modified_derived_consumer(self):
        self.run_target()
        for name in ALL_DERIVED:
            with self.subTest(name=name):
                path = self.consumer / name
                original = path.read_bytes()
                path.write_bytes(original + b'// unexpected\n')
                with self.assertRaisesRegex(RuntimeError, 'stale'):
                    self.run_target()
                path.write_bytes(original)

    def test_helper_anchors_reject_missing_and_duplicate_source_sites(self):
        sites = (
            (HELPERS[0], b'#include <Arduino.h>\n\n'),
            (HELPERS[0], b'  PulseOutput(uint8_t pin, ActiveLevel level = ActiveLevel::ActiveHigh);\n'),
            (HELPERS[0], b'private:\n  void writeLevel(bool onLogical);'),
            (HELPERS[0], b'  bool activeHigh_;\n'),
            (HELPERS[1], b'    : pin_(pin), activeHigh_(level == ActiveLevel::ActiveHigh) {\n'
                         b'  pinMode(pin_, OUTPUT);\n  writeLevel(false);\n'),
            (HELPERS[1], b'void PulseOutput::loop() {\n'),
            (HELPERS[1], b'void PulseOutput::writeLevel(bool onLogical) {\n'),
        )
        for name, anchor in sites:
            path = self.package / name
            original = path.read_bytes()
            for invalid in (original.replace(anchor, b'changed'), original + b'\n' + anchor + b'\n'):
                with self.subTest(name=name, invalid=invalid):
                    path.write_bytes(invalid)
                    self.refresh_manifest()
                    with self.assertRaisesRegex(RuntimeError, 'anchor'):
                        self.run_target()
            path.write_bytes(original)

    def test_helper_derivation_preserves_crlf(self):
        for name in HELPERS:
            path = self.package / name
            path.write_bytes(path.read_bytes().replace(b'\n', b'\r\n'))
        self.refresh_manifest()
        (self.consumer / frozen.MANIFEST).write_bytes((self.package / frozen.MANIFEST).read_bytes())
        for name in HELPERS:
            (self.consumer / name).write_bytes((self.package / name).read_bytes())
        self.run_target()
        for name in HELPERS:
            content = (self.consumer / name).read_bytes()
            self.assertNotIn(b'\n', content.replace(b'\r\n', b''))
            self.assertIn(b'\r\n', content)

    def test_requires_exactly_one_original_anchor_in_each_source(self):
        for content in (b'wrong anchor', ANCHOR + ANCHOR, PATCHED):
            with self.subTest(content=content):
                (self.package / NAMES[1]).write_bytes(content)
                self.refresh_manifest()
                with self.assertRaisesRegex(RuntimeError, 'anchor'):
                    self.run_target()

    def test_rejects_modified_frozen_source(self):
        (self.package / NAMES[1]).write_bytes(b'corrupt source')
        with self.assertRaisesRegex(RuntimeError, 'source package hash mismatch'):
            self.run_target()

    def test_rejects_unapproved_override_paths_and_content(self):
        for overrides in ({'tools/unchanged.py': b'bad'}, *({name: b'bad'} for name in ALL_DERIVED)):
            with self.subTest(overrides=overrides), self.assertRaises(RuntimeError):
                frozen.verify_development_package(self.package, self.consumer,
                                                  derived_content_overrides=overrides)

    def test_real_freeze_manifest_and_four_derived_files_match(self):
        package = Path(__file__).resolve().parents[2] / cm_registry.DEVELOPMENT_PIN.removeprefix('file://')
        frozen.verify_development_package(package, package)
        derived = c3_cm_portability.expected_content(package)
        self.assertEqual(set(derived), set(ALL_DERIVED))
        for name in NAMES:
            content = derived[name]
            self.assertEqual((package / name).read_bytes().count(ANCHOR), 1)
            self.assertEqual(content.count(PATCHED), 1)


if __name__ == '__main__':
    unittest.main()
