"""Use the installed ConfigManager package without modifying sibling projects."""

from pathlib import Path
import subprocess
import sys

Import("env")

library = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "ESP32 Configuration Manager"
header = library / "src" / "html_content.h"
generator = library / "tools" / "preCompile_script.py"

# Published packages already contain the matching WebUI asset. Generate only
# when it is absent; never mix a sibling checkout's WebUI with this version.
if header.is_file() and header.stat().st_size > 0:
    print("[I] ConfigManager packaged WebUI ready")
elif generator.is_file():
    subprocess.run([sys.executable, str(generator)], cwd=library, check=True)
    if not header.is_file() or header.stat().st_size == 0:
        raise RuntimeError("ConfigManager WebUI generation failed")
else:
    raise RuntimeError("ConfigManager package is missing its WebUI and generator")
