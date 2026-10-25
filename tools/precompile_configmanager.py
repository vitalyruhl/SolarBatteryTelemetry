"""Validate the pinned published CM package without modifying dependency sources."""
from pathlib import Path
import sys

Import("env")

sys.path.insert(0, str(Path(env.subst('$PROJECT_DIR')) / 'tools'))
from cm_registry import validate_environment, prepare_settings_ui
library = validate_environment(env)
asset_hash = prepare_settings_ui(env, library)
print('[I] Registry ConfigManager 4.9.0; WebUI Live0 Settings1; asset=' + asset_hash)
