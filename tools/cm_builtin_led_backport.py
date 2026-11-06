"""Backport only CM's board-aware, runtime-initialized pulse-output helper."""

from pathlib import Path


HEADER = 'src/helpers/HelperModule.h'
SOURCE = 'src/helpers/HelperModule.cpp'

# Each original site must occur once. Keep unrelated frozen bytes and line endings.
PATCHES = {
    HEADER: (
        ('#include <Arduino.h>\n\n', '''#include <Arduino.h>

#if !defined(CM_BUILTIN_LED_PIN) && defined(LED_BUILTIN)
#define CM_BUILTIN_LED_PIN LED_BUILTIN
#endif
#ifndef CM_BUILTIN_LED_ACTIVE_LOW
#define CM_BUILTIN_LED_ACTIVE_LOW 0
#endif

'''),
        ('  PulseOutput(uint8_t pin, ActiveLevel level = ActiveLevel::ActiveHigh);\n',
         '''  PulseOutput(uint8_t pin, ActiveLevel level = ActiveLevel::ActiveHigh);

#ifdef CM_BUILTIN_LED_PIN
  // Use the selected board's LED metadata, including Arduino virtual RGB pins.
  PulseOutput()
      : PulseOutput(CM_BUILTIN_LED_PIN, CM_BUILTIN_LED_ACTIVE_LOW
                                            ? ActiveLevel::ActiveLow
                                            : ActiveLevel::ActiveHigh) {}
#endif
'''),
        ('private:\n  void writeLevel(bool onLogical);',
         'private:\n  void initialize();\n  void writeLevel(bool onLogical);'),
        ('  bool activeHigh_;\n', '  bool activeHigh_;\n  bool initialized_ = false;\n'),
    ),
    SOURCE: (
        ('''    : pin_(pin), activeHigh_(level == ActiveLevel::ActiveHigh) {
  pinMode(pin_, OUTPUT);
  writeLevel(false);
''', '    : pin_(pin), activeHigh_(level == ActiveLevel::ActiveHigh) {\n'),
        ('void PulseOutput::loop() {\n', 'void PulseOutput::loop() {\n  initialize();\n'),
        ('void PulseOutput::writeLevel(bool onLogical) {\n',
         '''void PulseOutput::initialize() {
  if (initialized_) {
    return;
  }
  // Set the inactive latch before enabling output; never touch hardware in constructors.
  digitalWrite(pin_, activeHigh_ ? LOW : HIGH);
  pinMode(pin_, OUTPUT);
  initialized_ = true;
}

void PulseOutput::writeLevel(bool onLogical) {
  initialize();
'''),
    ),
}


def expected_content(package: Path) -> dict[str, bytes]:
    """Derive exactly the reviewed helper changes without importing another checkout."""
    result = {}
    for name, replacements in PATCHES.items():
        content = (package / name).read_bytes()
        newline = b'\r\n' if b'\r\n' in content else b'\n'
        for index, (original, derived) in enumerate(replacements):
            anchor = original.encode('utf-8').replace(b'\n', newline)
            if content.count(anchor) != 1:
                raise RuntimeError(f'CM builtin LED backport anchor mismatch: {name} site {index + 1}')
            content = content.replace(anchor, derived.encode('utf-8').replace(b'\n', newline), 1)
        result[name] = content
    return result
