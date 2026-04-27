# lsh-core AVR board matrix

This example is intentionally small. It exists to prove that `lsh-core` is not a
Controllino-only library and to keep representative Arduino AVR targets in CI.

The tested matrix is:

| Environment             | PlatformIO board | MCU        | Profile            | Fast I/O | Status                  |
| ----------------------- | ---------------- | ---------- | ------------------ | -------- | ----------------------- |
| `mega2560_fast_release` | `megaatmega2560` | ATmega2560 | `mega2560_fast`    | yes      | supported               |
| `uno_release`           | `uno`            | ATmega328P | `atmega328p_basic` | no       | supported, conservative |
| `nano_release`          | `nanoatmega328`  | ATmega328P | `atmega328p_basic` | no       | supported, conservative |

Controllino Maxi remains covered by `examples/multi-device-project`, because it
needs the Controllino library, pin aliases and optional board helpers.

Use this example as the starting point for a new non-Controllino AVR board:

1. Keep `preset = "arduino-generic/json"` for the first build.
2. Use numeric Arduino pins or symbols exposed by your board package.
3. Keep `fast_io = false` until the board is known to expose the standard
   Arduino AVR pin lookup tables.
4. Move to `fast_io = true` only after a release build and `pio check` pass.

Build locally from the repository root:

```bash
platformio run -d examples/avr-board-matrix -e mega2560_fast_release
platformio run -d examples/avr-board-matrix -e uno_release
platformio run -d examples/avr-board-matrix -e nano_release
```

Run static analysis:

```bash
platformio check -d examples/avr-board-matrix -e mega2560_fast_release --fail-on-defect low
platformio check -d examples/avr-board-matrix -e uno_release --fail-on-defect low
platformio check -d examples/avr-board-matrix -e nano_release --fail-on-defect low
```
