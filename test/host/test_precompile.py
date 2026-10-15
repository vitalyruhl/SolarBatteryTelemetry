"""[MOCKED!] Synthetic package metadata; no real credentials or dependency writes."""
import json
from pathlib import Path
import runpy
import shutil
import unittest
import uuid

ROOT=Path(__file__).resolve().parents[2]
class FakeEnv:
    def __init__(self,root,options):self.root=root;self.options=options
    def subst(self,value):return {"$PROJECT_LIBDEPS_DIR":str(self.root/"libdeps"),"$PIOENV":"usb"}[value]
    def GetProjectOption(self,key,default=None):return self.options.get(key,default)

class DependencyValidation(unittest.TestCase):
    def setUp(self):
        self.root=ROOT/".pio"/"host-tests"/("cm-fixture-"+uuid.uuid4().hex)
        self.library=self.root/"reviewed-cm"
        for folder in ["src","webui"]:(self.library/folder).mkdir(parents=True,exist_ok=True)
        (self.library/"library.json").write_text(json.dumps({"name":"ESP32 Configuration Manager","version":"4.5.0"}))
        (self.library/"src/ConfigManager.h").write_text('#define CONFIGMANAGER_VERSION "4.5.0"\n')
        (self.library/"src/html_content.h").write_text('// [MOCKED!] Coherent fixture UI asset\n')
        (self.library/"webui/package.json").write_text('{"version":"4.5.0"}')
        old=self.root/"libdeps/usb/ESP32 Configuration Manager/src";old.mkdir(parents=True)
        (old/"html_content.h").write_text('// [MOCKED!] Old cached package must never satisfy local override\n')
        self.options={"custom_cm_path":self.library.as_posix(),"lib_deps":["symlink://"+self.library.as_posix()]}
    def tearDown(self):
        target=self.root.resolve();allowed=(ROOT/".pio/host-tests").resolve()
        if target.parent!=allowed or not target.name.startswith("cm-fixture-"):raise RuntimeError("Unsafe fixture cleanup target")
        shutil.rmtree(target)
    def run_script(self):
        runpy.run_path(str(ROOT/"tools/precompile_configmanager.py"),init_globals={"Import":lambda name:None,"env":FakeEnv(self.root,self.options)})
    def test_missing_override_fails_even_with_old_cached_ui(self):
        self.options.pop("custom_cm_path")
        with self.assertRaisesRegex(RuntimeError,"local.*4.5") :self.run_script()
    def test_older_package_fails_early(self):
        (self.library/"library.json").write_text('{"name":"ESP32 Configuration Manager","version":"4.4.10"}')
        with self.assertRaisesRegex(RuntimeError,"4.5") :self.run_script()
    def test_version_mirror_mismatch_fails(self):
        (self.library/"webui/package.json").write_text('{"version":"4.4.10"}')
        with self.assertRaisesRegex(RuntimeError,"mirror") :self.run_script()
    def test_wrong_dependency_binding_fails(self):
        self.options["lib_deps"]=["registry-package@4.4.10"]
        with self.assertRaisesRegex(RuntimeError,"symlink") :self.run_script()
    def test_matching_reviewed_binding_is_read_only(self):
        before={p:p.read_bytes() for p in self.library.rglob('*') if p.is_file()}
        self.run_script()
        self.assertEqual(before,{p:p.read_bytes() for p in self.library.rglob('*') if p.is_file()})
if __name__=="__main__":unittest.main()
