"""Pin one worktree-local Espressif IDF4.4.7 controller; never modify the SDK."""

import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import urllib.request
import zipfile

VERSION = "0b40e99"
SOURCE_URL = "https://github.com/espressif/esp-idf/files/15496325/IDFGH_12768.20240530.zip"
ZIP_SHA256 = "621c70f029f522328907a5815ae43107b19b85725cd3e46a7e07e4a9e535c63f"
CANDIDATE_SHA256 = "3d890636991328a7a0aec387c876104372a1dabcd4920cec061268ecfe327b33"
CANDIDATE_SIZE = 621138
BASE_SHA256 = "981595473bda6953657f7504a6711d522fc112654759dd65bd9f9a07145b7078"
BASE_SIZE = 616480


def verify_file(path, expected_sha256, expected_size):
    path = Path(path)
    if not path.is_file():
        raise RuntimeError(f"Controller archive missing: {path}; run tools/bt_controller.py --prepare")
    payload = path.read_bytes()
    if len(payload) != expected_size:
        raise RuntimeError(f"Controller archive size mismatch: {path}")
    if hashlib.sha256(payload).hexdigest() != expected_sha256:
        raise RuntimeError(f"Controller archive SHA256 mismatch: {path}")


def validate_target(chip, framework_version):
    if chip != "esp32" or not framework_version.startswith("3.20017."):
        raise RuntimeError("Controller override requires classic ESP32 and Arduino-ESP32 2.0.17")


def select_member(names):
    suffix = f"4.4.7({VERSION})/libbtdm_app.a"
    matches = [name for name in names if name == suffix or name.endswith("/" + suffix)]
    if len(matches) != 1:
        raise RuntimeError("Official release member missing or ambiguous")
    return matches[0]


def candidate_path(project):
    return Path(project) / ".local" / "bt-controller" / VERSION / "libbtdm_app.a"


def prepare(project):
    destination = candidate_path(project)
    if destination.exists():
        verify_file(destination, CANDIDATE_SHA256, CANDIDATE_SIZE)
        print(f"[I] Controller {VERSION} already verified")
        return
    with urllib.request.urlopen(SOURCE_URL, timeout=30) as response:
        payload = response.read(2 * 1024 * 1024)
    if hashlib.sha256(payload).hexdigest() != ZIP_SHA256:
        raise RuntimeError("Official controller release ZIP SHA256 mismatch")
    with zipfile.ZipFile(io.BytesIO(payload)) as archive:
        member = select_member(archive.namelist())
        library = archive.read(member)
    if len(library) != CANDIDATE_SIZE or hashlib.sha256(library).hexdigest() != CANDIDATE_SHA256:
        raise RuntimeError("Official controller member SHA256 or size mismatch")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(".a.download")
    temporary.write_bytes(library)
    temporary.replace(destination)
    (destination.parent / "upstream-release.zip").write_bytes(payload)
    print(f"[I] Controller {VERSION} prepared and verified")


def verify_map(text, candidate, original, project=None):
    root = Path(project) if project is not None else Path.cwd()
    expected = candidate.resolve()
    # LOAD paths have no symbol/address columns; preserve every internal space.
    loads = [line.strip()[5:].strip() for line in text.splitlines()
             if line.strip().startswith("LOAD ") and line.strip().endswith("libbtdm_app.a")]
    if not loads:
        raise RuntimeError("Controller LOAD record absent from map")
    for item in loads:
        archive = Path(item.replace("\\", "/"))
        if (archive if archive.is_absolute() else root / archive).resolve() != expected:
            raise RuntimeError("Linker loaded an unverified controller archive")
    records = [line for line in text.splitlines() if "libbtdm_app.a(" in line]
    if not records:
        raise RuntimeError("Linker controller archive absent from map")
    for record in records:
        prefix = record.split("libbtdm_app.a(", 1)[0].strip()
        # GNU map records may prefix a path with a symbol/section and hexadecimal fields.
        prefix = re.sub(r"^(?:\S+\s+)?(?:0x[0-9a-fA-F]+\s+){1,2}", "", prefix)
        prefix = re.sub(r"^[^\s/\\:]+\s{2,}", "", prefix)
        archive = Path((prefix + "libbtdm_app.a").replace("\\", "/"))
        actual = (archive if archive.is_absolute() else root / archive).resolve()
        if actual != expected:
            raise RuntimeError(f"Linker selected an unverified controller archive: {actual}")


def configure(env):
    project = Path(env.subst("$PROJECT_DIR"))
    framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
    package = json.loads((framework / "package.json").read_text(encoding="utf-8"))
    validate_target(env.BoardConfig().get("build.mcu"), package["version"])
    original = framework / "tools" / "sdk" / "esp32" / "ld" / "libbtdm_app.a"
    candidate = candidate_path(project)
    verify_file(original, BASE_SHA256, BASE_SIZE)
    verify_file(candidate, CANDIDATE_SHA256, CANDIDATE_SIZE)
    env.Prepend(LIBPATH=[str(candidate.parent)])
    print(f"[I] Controller override {VERSION}; shared SDK verified unchanged")

    def check_linked(source, target, env):
        elf = Path(target[0].get_abspath())
        verify_map(elf.with_suffix(".map").read_text(encoding="utf-8"), candidate, original, project)
        if (VERSION + "\0").encode("ascii") not in elf.read_bytes():
            raise RuntimeError("Pinned controller identity missing from linked ELF")
        verify_file(original, BASE_SHA256, BASE_SIZE)
        verify_file(candidate, CANDIDATE_SHA256, CANDIDATE_SIZE)
        evidence = {
            "controller_version": VERSION,
            "controller_sha256": CANDIDATE_SHA256,
            "shared_sdk_sha256": BASE_SHA256,
            "shared_sdk_unchanged": True,
            "linked_archive": str(candidate),
            "upstream_url": SOURCE_URL,
            "upstream_zip_sha256": ZIP_SHA256,
            "elf_sha256": hashlib.sha256(elf.read_bytes()).hexdigest(),
        }
        (elf.parent / "controller-provenance.json").write_text(json.dumps(evidence, indent=2) + "\n")
        print(f"[I] Linked controller {VERSION} verified")

    env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", check_linked)


if "Import" in globals():
    Import("env")
    configure(env)
elif __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepare", action="store_true", required=True)
    parser.add_argument("--project", type=Path, default=Path(__file__).resolve().parents[1])
    prepare(parser.parse_args().project)
