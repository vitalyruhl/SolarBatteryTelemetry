"""Validate the reviewed local CM development dependency without modifying it."""
import json
from pathlib import Path
import re

Import("env")

hint="Set custom_cm_path and an absolute symlink:// dependency in ignored platformio.local.ini; reviewed local CM 4.7.0 is unpublished."
configured=env.GetProjectOption("custom_cm_path","")
if not configured:
    raise RuntimeError("Missing local ConfigManager 4.7 override. "+hint)
library=Path(configured)
if not library.is_absolute():
    raise RuntimeError("ConfigManager local path must be absolute. "+hint)
library=library.resolve()
dependencies=env.GetProjectOption("lib_deps",[])
if isinstance(dependencies,str):dependencies=dependencies.splitlines()
links=[Path(value.strip()[len("symlink://"):]).resolve() for value in dependencies if value.strip().startswith("symlink://")]
if library not in links:
    raise RuntimeError("ConfigManager symlink dependency does not match custom_cm_path. "+hint)
manifest=library/"library.json"
if not manifest.is_file():raise RuntimeError("Local ConfigManager manifest is missing. "+hint)
metadata=json.loads(manifest.read_text(encoding="utf-8"))
version=metadata.get("version","")
parsed=re.fullmatch(r"(\d+)\.(\d+)\.(\d+)",version)
if metadata.get("name")!="ESP32 Configuration Manager" or not parsed or tuple(map(int,parsed.groups()))<(4,7,0):
    raise RuntimeError("ConfigManager 4.7.0 or newer reviewed development source is required. "+hint)
header=(library/"src/ConfigManager.h").read_text(encoding="utf-8")
match=re.search(r'^#define\s+CONFIGMANAGER_VERSION\s+"([^"]+)"',header,re.MULTILINE)
web=json.loads((library/"webui/package.json").read_text(encoding="utf-8"))
if not match or match.group(1)!=version or web.get("version")!=version:
    raise RuntimeError("ConfigManager version mirror mismatch; use one coherent reviewed checkout.")
asset=library/"src/html_content.h"
if not asset.is_file() or not asset.stat().st_size:
    raise RuntimeError("Reviewed local ConfigManager WebUI asset is missing; prepare that checkout separately.")
# Never regenerate or copy assets from another package; reviewed CM stays unchanged.
print("[I] Reviewed local ConfigManager "+version+"; matching WebUI ready")
