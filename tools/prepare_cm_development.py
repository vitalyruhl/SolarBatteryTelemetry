"""Create a bounded local CM development package without modifying its checkout."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess


EXCLUDED = {"node_modules", ".pio", ".git", ".local", ".temp", "__pycache__", ".cache", ".vite"}
ROOT_FILES = ("library.json", "README.md", "README", "README.txt", "LICENSE", "LICENSE.md", "LICENSE.txt")
TREE_ROOTS = ("src", "tools", "webui")
MANIFEST = "manifest.sha"
VERSION = "4.10.1"
PACKAGE = "cm-wifi-coex-4.10.1-package"


def is_reparse(path: Path) -> bool:
    return path.is_symlink() or (
        path.exists()
        and bool(getattr(path.lstat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT)
    )


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def files_under(root: Path, relative: Path = Path(), *, package_check: bool = False) -> list[Path]:
    """Walk only an explicitly selected tree and never follow symbolic links."""
    result = []
    with os.scandir(root / relative) as entries:
        for entry in sorted(entries, key=lambda item: item.name):
            child = relative / entry.name
            if entry.is_symlink() or getattr(entry.stat(follow_symlinks=False), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT:
                if package_check:
                    raise RuntimeError("Existing package contains a symbolic link or junction")
                continue
            if entry.is_dir(follow_symlinks=False):
                if not package_check and entry.name.casefold() in EXCLUDED:
                    continue
                if not package_check and child.as_posix().casefold() == "tools/_buildlogs":
                    continue
                result.extend(files_under(root, child, package_check=package_check))
            elif entry.is_file(follow_symlinks=False):
                result.append(child)
    return result


def source_files(source: Path) -> list[Path]:
    selected = []
    for name in ROOT_FILES:
        candidate = source / name
        if candidate.is_file() and not is_reparse(candidate):
            selected.append(Path(name))
    for name in TREE_ROOTS:
        candidate = source / name
        if is_reparse(candidate):
            raise RuntimeError(f"Refusing symbolic-link source tree: {name}")
        if not candidate.is_dir():
            raise RuntimeError(f"Required source tree missing: {name}")
        selected.extend(files_under(source, Path(name)))
    if Path("library.json") not in selected:
        raise RuntimeError("Required library.json missing")
    return sorted(selected, key=lambda item: item.as_posix())


def read_manifest(package: Path) -> dict[str, str]:
    try:
        lines = (package / MANIFEST).read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise RuntimeError("CM package manifest missing") from error
    if f"# source_version={VERSION}" not in lines:
        raise RuntimeError("CM package manifest version mismatch")
    result = {}
    for line in lines:
        if line.startswith("#"):
            continue
        checksum, separator, name = line.partition("  ")
        path = PurePosixPath(name)
        if (not separator or not re.fullmatch(r"[0-9a-f]{64}", checksum) or
                not name or "\\" in name or ":" in name or path.is_absolute() or
                any(part in (".", "..") for part in path.parts) or
                path.as_posix() != name or name in result):
            raise RuntimeError("Invalid CM package manifest entry")
        result[name] = checksum
    if not {"library.json", "src/ConfigManager.h", "webui/package.json"}.issubset(result):
        raise RuntimeError("CM package manifest lacks required sources")
    return result


def generated_directory(path: Path) -> bool:
    name = path.as_posix()
    return (name in (".pio", "webui/dist", "webui/node_modules", "webui/.vite") or
            (path.parts[0] == "tools" and path.name == "__pycache__"))


def consumed_files(root: Path, relative: Path = Path()) -> set[str]:
    result = set()
    with os.scandir(root / relative) as entries:
        for entry in entries:
            child = relative / entry.name
            if generated_directory(child):
                if is_reparse(root / child):
                    raise RuntimeError("Generated CM directory must stay inside the consumed package")
                continue
            if is_reparse(root / child):
                raise RuntimeError("Consumed CM source contains a symbolic link or junction")
            if entry.is_dir(follow_symlinks=False):
                result.update(consumed_files(root, child))
            elif entry.is_file(follow_symlinks=False):
                if child.as_posix() not in (MANIFEST, ".piopm", "src/html_content.h"):
                    result.add(child.as_posix())
    return result


def mutable_ui_input(name: str) -> bool:
    return name == "src/html_content.h" or name.startswith("webui/dist/")


def verify_development_package(package: Path, consumed: Path) -> None:
    """Frozen package is exact; consumer may regenerate only derived UI/cache files."""
    if is_reparse(package) or is_reparse(package.parent):
        raise RuntimeError("CM source package must not be a symbolic link")
    expected = read_manifest(package)
    actual = {path.as_posix() for path in files_under(package, package_check=True)}
    if actual != set(expected) | {MANIFEST}:
        raise RuntimeError("CM source package differs from its manifest")
    for name, checksum in expected.items():
        if digest(package / name) != checksum:
            raise RuntimeError(f"CM source package hash mismatch: {name}")
    if (consumed / MANIFEST).read_bytes() != (package / MANIFEST).read_bytes():
        raise RuntimeError("Consumed CM manifest differs from frozen source package")
    immutable = {name for name in expected if not mutable_ui_input(name)}
    if consumed_files(consumed) != immutable:
        raise RuntimeError("Consumed CM source tree differs from manifest")
    for name in immutable:
        if digest(consumed / name) != expected[name]:
            raise RuntimeError(f"Consumed CM source is stale: {name}")


def copy_frozen(source: Path, destination: Path) -> int:
    """Copy one verified standalone CM freeze; never recompute its Git provenance."""
    verify_development_package(source, source)
    if destination.exists():
        verify_development_package(destination, destination)
        if (destination / MANIFEST).read_bytes() != (source / MANIFEST).read_bytes():
            raise RuntimeError("Existing SBT package differs; choose a new reviewed target")
    else:
        destination.mkdir(parents=True, exist_ok=False)
        for relative in files_under(source, package_check=True):
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / relative, target, follow_symlinks=False)
    verify_development_package(source, destination)
    return len(read_manifest(source))


def main() -> None:
    repository = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-package', type=Path,
                        default=repository.parent / 'ConfigurationsManager/.local' / PACKAGE,
                        help='Existing immutable CM source freeze; no live checkout import')
    source = parser.parse_args().source_package
    destination = repository / ".local" / PACKAGE
    count = copy_frozen(source, destination)
    print(f"[I] Frozen CM {VERSION} copied and verified; files={count}")


if __name__ == "__main__":
    main()
