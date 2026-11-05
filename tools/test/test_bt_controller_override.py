"""[MOCKED!] Synthetic build inputs; no devices or shared SDK files are changed."""

import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest

MODULE_PATH = Path(__file__).resolve().parents[1] / "bt_controller.py"
SPEC = importlib.util.spec_from_file_location("bt_controller", MODULE_PATH)
controller = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(controller)


class ControllerOverrideTests(unittest.TestCase):
    def test_missing_archive_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaisesRegex(RuntimeError, "missing"):
                controller.verify_file(Path(folder) / "missing.a", "0" * 64, 8)

    def test_truncated_or_modified_archive_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "lib.a"
            path.write_bytes(b"modified")
            with self.assertRaisesRegex(RuntimeError, "size"):
                controller.verify_file(path, "0" * 64, 99)
            with self.assertRaisesRegex(RuntimeError, "SHA256"):
                controller.verify_file(path, "0" * 64, len(b"modified"))

    def test_verified_file_remains_unchanged(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "lib.a"
            payload = b"!<arch>\nfixture"
            path.write_bytes(payload)
            controller.verify_file(path, hashlib.sha256(payload).hexdigest(), len(payload))
            self.assertEqual(path.read_bytes(), payload)

    def test_wrong_chip_or_framework_fails(self):
        for chip, framework in [("esp32s3", "3.20017.0"), ("esp32", "3.30000.0")]:
            with self.subTest(chip=chip, framework=framework):
                with self.assertRaises(RuntimeError):
                    controller.validate_target(chip, framework)
        controller.validate_target("esp32", "3.20017.241212")

    def test_linker_must_select_candidate_archive(self):
        candidate = Path("C:/fixture with spaces/.local/bt/libbtdm_app.a")
        sdk = Path("C:/sdk/ld/libbtdm_app.a")
        good = "LOAD " + str(candidate) + "\n" + str(candidate) + "(arch_main.o)\n"
        controller.verify_map(good.replace("/", "\\"), candidate, sdk)
        for bad in [str(sdk) + "(arch_main.o)", good + str(sdk) + "(other.o)", ""]:
            with self.subTest(map=bad):
                with self.assertRaises(RuntimeError):
                    controller.verify_map(bad, candidate, sdk)

    def test_release_zip_must_contain_only_the_expected_target_member(self):
        with self.assertRaisesRegex(RuntimeError, "member"):
            controller.select_member(["4.4.4(e85d841)/libbtdm_app.a"])
        member = "IDFGH_12768(20240530)/4.4.7(0b40e99)/libbtdm_app.a"
        self.assertEqual(controller.select_member([member]), member)
        with self.assertRaisesRegex(RuntimeError, "member"):
            controller.select_member([member, "other/4.4.7(0b40e99)/libbtdm_app.a"])

    def test_linker_relative_paths_are_resolved_against_project(self):
        project = Path("C:/fixture")
        candidate = project / ".local/bt/libbtdm_app.a"
        sdk = Path("C:/sdk/ld/libbtdm_app.a")
        controller.verify_map("LOAD .local\\bt\\libbtdm_app.a\n.local\\bt\\libbtdm_app.a(arch_main.o)", candidate, sdk, project)

    def test_other_worktree_with_same_archive_suffix_is_rejected(self):
        project = Path("C:/fixture")
        candidate = project / ".local/bt-controller/0b40e99/libbtdm_app.a"
        sdk = Path("C:/sdk/ld/libbtdm_app.a")
        wrong = "C:\\Temp\\stale-other-project\\.local\\bt-controller\\0b40e99\\libbtdm_app.a(arch_main.o)"
        with self.assertRaises(RuntimeError):
            controller.verify_map("LOAD " + wrong.split("(")[0] + "\n" + wrong, candidate, sdk, project)

    def test_ambiguous_space_path_is_rejected_by_load_record(self):
        project = Path("C:/fixture")
        candidate = project / ".local/bt-controller/0b40e99/libbtdm_app.a"
        sdk = Path("C:/sdk/ld/libbtdm_app.a")
        wrong = "stale  .local/bt-controller/0b40e99/libbtdm_app.a"
        with self.assertRaises(RuntimeError):
            controller.verify_map("LOAD " + wrong + "\n" + wrong + "(arch_main.o)", candidate, sdk, project)


if __name__ == "__main__":
    unittest.main()
