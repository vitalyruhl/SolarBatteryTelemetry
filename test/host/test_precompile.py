"""[MOCKED!] Registry package metadata; no credentials or dependency writes."""
import json
import hashlib
from pathlib import Path
import runpy
import shutil
import unittest
from unittest.mock import patch
import uuid

ROOT=Path(__file__).resolve().parents[2]
PIN='vitaly.ruhl/ESP32 Configuration Manager@4.9.0'
class FakeEnv:
    def __init__(self,root,options):self.root=root;self.options=options
    def subst(self,value):return {"$PROJECT_DIR":str(ROOT),"$PROJECT_LIBDEPS_DIR":str(self.root/"libdeps"),"$PIOENV":"usb"}[value]
    def GetProjectOption(self,key,default=None):return self.options.get(key,default)

class DependencyValidation(unittest.TestCase):
    def setUp(self):
        self.root=ROOT/".pio"/"host-tests"/("cm-fixture-"+uuid.uuid4().hex)
        self.library=self.root/"libdeps/usb/ESP32 Configuration Manager"
        for folder in ["src","webui","tools",".pio"]:(self.library/folder).mkdir(parents=True,exist_ok=True)
        (self.library/"library.json").write_text(json.dumps({"name":"ESP32 Configuration Manager","version":"4.9.0"}))
        (self.library/"src/ConfigManager.h").write_text('#define CONFIGMANAGER_VERSION "4.9.0"\n')
        (self.library/"src/html_content.h").write_text('#define CM_WEBUI_LIVE_UI 0\n#define CM_WEBUI_SETTINGS_UI 1\n')
        (self.library/"webui/package.json").write_text('{"version":"4.9.0"}')
        (self.library/'.piopm').write_text(json.dumps({'version':'4.9.0','spec':{'owner':'vitaly.ruhl'}}))
        features={'CM_ENABLE_LIVE_UI':'0','CM_ENABLE_SETTINGS_UI':'1','CM_EMBED_WEBUI':'1'}
        (self.library/'tools/webui_features.py').write_text('def feature_values(env): return '+repr(features)+'\ndef source_fingerprint(root, flags): return "fixture"\n')
        (self.library/'tools/preCompile_script.py').write_text('# [MOCKED!] Process invocation is intercepted by this unit test.\n')
        (self.library/'.pio/cm-webui-build.json').write_text(json.dumps({'features':features,'fingerprint':'fixture','header_sha256':hashlib.sha256((self.library/'src/html_content.h').read_bytes()).hexdigest()}))
        self.options={"lib_deps":[PIN]}
    def tearDown(self):
        target=self.root.resolve();allowed=(ROOT/".pio/host-tests").resolve()
        if target.parent!=allowed or not target.name.startswith("cm-fixture-"):raise RuntimeError("Unsafe fixture cleanup target")
        shutil.rmtree(target)
    def run_script(self):
        with patch('subprocess.run'):
            runpy.run_path(str(ROOT/"tools/precompile_configmanager.py"),init_globals={"Import":lambda name:None,"env":FakeEnv(self.root,self.options)})
    def test_exact_registry_pin_required(self):
        self.options['lib_deps']=['vitaly.ruhl/ESP32 Configuration Manager@^4.9.0']
        with self.assertRaisesRegex(RuntimeError,'exact.*4.9.0'):self.run_script()
    def test_local_override_rejected(self):
        self.options['custom_cm_path']=self.library.as_posix()
        with self.assertRaisesRegex(RuntimeError,'local|Local'):self.run_script()
    def test_older_package_fails_early(self):
        (self.library/"library.json").write_text('{"name":"ESP32 Configuration Manager","version":"4.4.10"}')
        with self.assertRaisesRegex(RuntimeError,"4.9.0") :self.run_script()
    def test_unpublished_package_rejected(self):
        (self.library/"library.json").write_text('{"name":"ESP32 Configuration Manager","version":"4.9.1"}')
        (self.library/"src/ConfigManager.h").write_text('#define CONFIGMANAGER_VERSION "4.9.1"\n')
        (self.library/"webui/package.json").write_text('{"version":"4.9.1"}')
        with self.assertRaisesRegex(RuntimeError,"4.9.0") :self.run_script()
    def test_version_mirror_mismatch_fails(self):
        (self.library/"webui/package.json").write_text('{"version":"4.4.10"}')
        with self.assertRaisesRegex(RuntimeError,"mirror") :self.run_script()
    def test_symlink_binding_rejected(self):
        self.options["lib_deps"]=[PIN,"symlink://C:/unpublished/ConfigurationsManager"]
        with self.assertRaisesRegex(RuntimeError,"symlink") :self.run_script()
    def test_matching_published_package_is_read_only(self):
        before={p:p.read_bytes() for p in self.library.rglob('*') if p.is_file()}
        self.run_script()
        self.assertEqual(before,{p:p.read_bytes() for p in self.library.rglob('*') if p.is_file() and '__pycache__' not in p.parts})
    def test_generated_profile_mismatch_rejected(self):
        (self.library/'src/html_content.h').write_text('#define CM_WEBUI_LIVE_UI 1\n#define CM_WEBUI_SETTINGS_UI 1\n')
        with self.assertRaisesRegex(RuntimeError,'profile'):self.run_script()
    def test_generated_asset_proof_mismatch_rejected(self):
        (self.library/'.pio/cm-webui-build.json').write_text('{}')
        with self.assertRaisesRegex(RuntimeError,'proof'):self.run_script()
if __name__=="__main__":unittest.main()
