"""Resolve explicit published or frozen development CM bindings and Settings assets."""
import json
import hashlib
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys

PIN = 'vitaly.ruhl/ESP32 Configuration Manager@4.9.0'
VERSION = '4.9.0'
DEVELOPMENT_PIN = 'file://.local/cm-wifi-coex-4.10.1-package'
DEVELOPMENT_VERSION = '4.10.1'


def development_package(env):
    library = Path(env.subst('$PROJECT_LIBDEPS_DIR')) / env.subst('$PIOENV') / 'ESP32 Configuration Manager'
    if library.resolve() != library.absolute():
        raise RuntimeError('Development CM must be a consumer-local copy, not a source symlink')
    metadata = json.loads((library / 'library.json').read_text(encoding='utf-8-sig'))
    if metadata.get('name') != 'ESP32 Configuration Manager' or metadata.get('version') != DEVELOPMENT_VERSION:
        raise RuntimeError('Development ConfigManager manifest version must be 4.10.1')
    provenance = json.loads((library / '.piopm').read_text(encoding='utf-8-sig'))
    if provenance.get('version') != DEVELOPMENT_VERSION or provenance.get('spec', {}).get('uri') != DEVELOPMENT_PIN:
        raise RuntimeError('Development ConfigManager URI/version provenance mismatch')
    header = (library / 'src/ConfigManager.h').read_text(encoding='utf-8-sig')
    match = re.search(r'^#define\s+CONFIGMANAGER_VERSION\s+"([^"]+)"', header, re.M)
    web = json.loads((library / 'webui/package.json').read_text(encoding='utf-8-sig'))
    if not match or match[1] != DEVELOPMENT_VERSION or web.get('version') != DEVELOPMENT_VERSION:
        raise RuntimeError('Development ConfigManager version mirror mismatch')
    from prepare_cm_development import verify_development_package
    source = Path(env.subst('$PROJECT_DIR')) / DEVELOPMENT_PIN.removeprefix('file://')
    verify_development_package(source, library)
    return library

def registry_package(libdeps, environment='usb'):
    library = Path(libdeps) / environment / 'ESP32 Configuration Manager'
    if library.resolve() != library.absolute():
        raise RuntimeError('Local/symlink ConfigManager package is not a Registry binding')
    manifest = library / 'library.json'
    if not manifest.is_file():
        raise RuntimeError('Registry CM 4.9.0 is missing; resolve the project dependencies first')
    metadata = json.loads(manifest.read_text(encoding='utf-8-sig'))
    if metadata.get('name') != 'ESP32 Configuration Manager' or metadata.get('version') != VERSION:
        raise RuntimeError('Only the published Registry CM 4.9.0 is allowed')
    provenance = json.loads((library / '.piopm').read_text(encoding='utf-8-sig'))
    if provenance.get('version') != VERSION or provenance.get('spec', {}).get('owner') != 'vitaly.ruhl':
        raise RuntimeError('Registry ConfigManager owner/version provenance mismatch')
    header = (library / 'src/ConfigManager.h').read_text(encoding='utf-8-sig')
    match = re.search(r'^#define\s+CONFIGMANAGER_VERSION\s+"([^"]+)"', header, re.M)
    web = json.loads((library / 'webui/package.json').read_text(encoding='utf-8-sig'))
    if not match or match[1] != VERSION or web.get('version') != VERSION:
        raise RuntimeError('Registry ConfigManager version mirror mismatch')
    asset = library / 'src/html_content.h'
    if not asset.is_file() or not asset.stat().st_size:
        raise RuntimeError('Published Registry ConfigManager UI asset is missing')
    return library

def validate_environment(env):
    if env.GetProjectOption('custom_cm_path', ''):
        raise RuntimeError('Local ConfigManager overrides are not allowed; use the exact Registry pin')
    dependencies = env.GetProjectOption('lib_deps', [])
    if isinstance(dependencies, str):
        dependencies = dependencies.splitlines()
    if any(str(value).strip().startswith('symlink://') for value in dependencies):
        raise RuntimeError('symlink dependencies are not allowed for the Registry build')
    selected = [str(value).strip() for value in dependencies]
    if DEVELOPMENT_PIN in selected:
        if selected.count(DEVELOPMENT_PIN) != 1 or PIN in selected:
            raise RuntimeError('Select exactly one explicit ConfigManager binding')
        return development_package(env)
    if selected.count(PIN) != 1:
        raise RuntimeError('The exact Registry CM 4.9.0 pin is required')
    return registry_package(env.subst('$PROJECT_LIBDEPS_DIR'), env.subst('$PIOENV'))

def prepare_settings_ui(env, library):
    # The official generator changes only the consumer-local package's derived assets.
    spec = importlib.util.spec_from_file_location('sbt_registry_webui_features', library / 'tools/webui_features.py')
    features_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(features_module)
    features = features_module.feature_values(env)
    if (features['CM_ENABLE_LIVE_UI'], features['CM_ENABLE_SETTINGS_UI'], features['CM_EMBED_WEBUI']) != ('0', '1', '1'):
        raise RuntimeError('Settings-only WebUI flags are required: Live0 Settings1 embedded')
    process_env = os.environ.copy()
    process_env.update(features)
    process_env.update({'VITE_' + key: value for key, value in features.items()})
    process_env.update(NODE_ENV='production', NPM_CONFIG_INCLUDE='dev')
    subprocess.run([sys.executable, str(library / 'tools/preCompile_script.py')], cwd=str(library), env=process_env, check=True)
    if DEVELOPMENT_PIN in env.GetProjectOption('lib_deps', []):
        development_package(env)
    asset = library / 'src/html_content.h'
    text = asset.read_text(encoding='utf-8-sig')
    for macro, expected in (('CM_WEBUI_LIVE_UI', '0'), ('CM_WEBUI_SETTINGS_UI', '1')):
        if not re.search(r'^#define\s+' + macro + r'\s+' + expected + r'\s*$', text, re.M):
            raise RuntimeError('Generated ConfigManager UI profile does not match Settings-only flags')
    proof = json.loads((library / '.pio/cm-webui-build.json').read_text(encoding='utf-8-sig'))
    fingerprint = features_module.source_fingerprint(library, {**features, 'NODE_ENV': 'production'})
    asset_hash = hashlib.sha256(asset.read_bytes()).hexdigest()
    if proof.get('fingerprint') != fingerprint or proof.get('features') != features or proof.get('header_sha256') != asset_hash:
        raise RuntimeError('Generated ConfigManager UI source/flags/hash proof mismatch')
    return asset_hash
