"""Derive bounded ESP32-C3 type/LED fixes; keep CM's freeze exact."""

from pathlib import Path


FILES = ('src/ConfigManager.h', 'src/core/CoreSettings.h')
ORIGINAL = b'std::min(9999u, static_cast<uint32_t>(minutes))'
DERIVED = b'std::min<uint32_t>(9999u, static_cast<uint32_t>(minutes))'


def expected_content(package: Path) -> dict[str, bytes]:
    """Require one exact original expression per file, preserving all other bytes."""
    result = {}
    for name in FILES:
        original = (package / name).read_bytes()
        if original.count(ORIGINAL) != 1 or DERIVED in original:
            raise RuntimeError(f'CM C3 portability anchor mismatch: {name}')
        result[name] = original.replace(ORIGINAL, DERIVED, 1)
    from cm_builtin_led_backport import expected_content as builtin_led_content
    result.update(builtin_led_content(package))
    return result


def prepare_consumer(package: Path, consumed: Path) -> dict[str, bytes]:
    """Validate all inputs before writing, accepting only original or exact derived bytes."""
    from prepare_cm_development import verify_development_package

    verify_development_package(package, package)
    derived = expected_content(package)
    existing = {name: content for name, content in derived.items()
                if (consumed / name).read_bytes() == content}
    verify_development_package(package, consumed, derived_content_overrides=existing)
    for name, content in derived.items():
        if name not in existing:
            (consumed / name).write_bytes(content)
    verify_development_package(package, consumed, derived_content_overrides=derived)
    return derived
