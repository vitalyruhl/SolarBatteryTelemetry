"""Validate the explicit CM binding and regenerate only consumer-local UI assets."""
from pathlib import Path
import json
import sys

Import("env")

sys.path.insert(0, str(Path(env.subst('$PROJECT_DIR')) / 'tools'))
sys.dont_write_bytecode = True
from cm_registry import validate_environment, prepare_settings_ui
library = validate_environment(env)
asset_hash = prepare_settings_ui(env, library)
version = json.loads((library / 'library.json').read_text(encoding='utf-8-sig'))['version']
print('[I] Explicit ConfigManager ' + version + '; WebUI Live0 Settings1; asset=' + asset_hash)
