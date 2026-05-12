# lsh-core: Controller Firmware for Labo Smart Home

[![PlatformIO Registry](https://badges.registry.platformio.org/packages/labodj/library/lsh-core.svg)](https://registry.platformio.org/libraries/labodj/lsh-core)
[![CI](https://github.com/labodj/lsh-core/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/labodj/lsh-core/actions/workflows/ci.yml)
[![Latest Release](https://img.shields.io/github/v/release/labodj/lsh-core?display_name=tag&sort=semver)](https://github.com/labodj/lsh-core/releases/latest)
[![API Documentation](https://img.shields.io/badge/API%20Reference-Doxygen-blue.svg)](https://labodj.github.io/lsh-core/)
[![License](https://img.shields.io/github/license/labodj/lsh-core.svg)](https://github.com/labodj/lsh-core/blob/main/LICENSE)

`lsh-core` is the controller-side firmware library for the **Labo Smart Home** stack. It
runs on an Arduino-compatible controller, owns the physical inputs and outputs, handles
local behavior for buttons, relays and indicators, and talks to an ESP32 `lsh-bridge`
over serial.

The path documented in most detail is a Controllino-style AVR controller paired with
`lsh-bridge`, MQTT and the LSH coordinator. You can still inspect or reuse the library
on its own, but the public documentation uses that full stack as the reference path.

Since `v3.0.0`, configuration is TOML-first. You describe devices, buttons, relays, pins
and click behavior in `lsh_devices.toml`; the generator emits the optimized C++ profile
before compilation. Most device profiles do not need hand-written topology code.

If you are new to LSH as a whole, start with the
[`labo-smart-home` documentation map](https://github.com/labodj/labo-smart-home/blob/main/DOCS.md)
before diving into firmware details.

## What lsh-core Owns

`lsh-core` is intentionally close to the physical panel:

- it reads wired inputs
- it drives relays and indicators
- it keeps local behavior usable when the network is unavailable
- it emits controller topology and state to the bridge
- it applies local fallback when a network-assisted click cannot complete

The bridge, MQTT broker and coordinator add visibility and distributed behavior. The
physical panel does not depend on those layers for its local loop.

## What You Need

For the documented controller path:

- PlatformIO
- Python 3.11 or newer for the TOML generator
- an Arduino-compatible AVR target; Controllino Maxi is documented in the most detail
- an ESP32 running `lsh-bridge` if you want MQTT/Homie integration
- an MQTT broker plus the LSH coordinator or Node-RED logic layer for distributed
  behavior

The library is optimized for static device topology. If your project needs devices to
appear and disappear at runtime, plan for additional runtime orchestration outside
`lsh-core`.

## First Build

The practical starting reference is the bundled multi-device example:

```bash
platformio run -d examples/multi-device-project -e J1_release
platformio run -d examples/multi-device-project -e J2_release
```

Use `J1_release` first when you want a lean controller/bridge path without network-click
behavior. Use `J2_release` when you want network-click support enabled.

For the stack-level bring-up sequence around this example, use the
[`labo-smart-home` getting started guide](https://github.com/labodj/labo-smart-home/blob/main/GETTING_STARTED.md).

## Create a Consumer Project

Install from the PlatformIO Registry:

```ini
[env:kitchen_release]
platform = atmelavr
framework = arduino
board = controllino_maxi
lib_deps = labodj/lsh-core @ ^3.2.0
build_unflags = -std=gnu++11 -std=c++11
build_flags =
    -I include
    -std=gnu++17
```

Add the static-config pre-build hook:

```ini
extra_scripts = pre:.pio/libdeps/kitchen_release/lsh-core/tools/platformio_lsh_static_config.py
custom_lsh_config = lsh_devices.toml
custom_lsh_device = kitchen
```

Then write `lsh_devices.toml` and build. PlatformIO runs the generator, validates the
TOML, writes the generated headers, adds the selected `LSH_BUILD_*` macro and compiles
the matching static profile.

For a complete layout, reuse the structure of
[examples/multi-device-project](https://github.com/labodj/lsh-core/tree/main/examples/multi-device-project)
instead of starting from an empty project.

If you are building the full LSH stack, the easier path is to let the `labo-smart-home`
stack composer generate the controller PlatformIO environments for you. Add the
generated `platformio-core.ini` with PlatformIO `extra_configs`, then build the
generated environment for the target controller:

```ini
[platformio]
extra_configs = generated/platformio-core.ini
```

```bash
platformio run -e core_kitchen
```

In the one-project workflow, `lsh_devices.toml` stays in your installation project and
`lsh-core` stays a normal PlatformIO dependency. The generated environments avoid
hand-maintained per-device blocks.

## Configuration Model

User-authored configuration lives in TOML:

```toml
schema_version = 2
preset = "controllino-maxi/fast-msgpack"

[controller]
debug_serial = "Serial"
bridge_serial = "Serial2"

[devices.kitchen]
name = "kitchen"

[devices.kitchen.actuators.ceiling]
pin = "R0"
auto_off = "30m"

[devices.kitchen.buttons.door]
pin = "A0"
short = "ceiling"
long = { after = "900ms", action = "off", target = "ceiling" }
super_long = { action = "all_off" }

[devices.kitchen.indicators.ceiling_led]
pin = "D0"
when = "ceiling"
```

Generated headers are an implementation detail. Edit `lsh_devices.toml`, regenerate and
build. When IDs are auto-assigned, commit `lsh_devices.lock.toml` beside the TOML
profile so public bridge-facing IDs stay stable over time.

Read
[docs/static-toml-config.md](https://github.com/labodj/lsh-core/blob/main/docs/static-toml-config.md)
for the schema reference and
[docs/cookbook.md](https://github.com/labodj/lsh-core/blob/main/docs/cookbook.md) for
copyable patterns.

## Board Support

Controllino is the hardware path documented in most detail, but it is not required. The
public compatibility matrix also keeps generic Arduino AVR targets in CI.

| Board / family          | PlatformIO board   | Example environment                      | Recommended profile                 |
| ----------------------- | ------------------ | ---------------------------------------- | ----------------------------------- |
| Controllino Maxi        | `controllino_maxi` | `multi-device-project:J1_release`        | `controllino-maxi/fast-msgpack`     |
| Arduino Mega 2560       | `megaatmega2560`   | `avr-board-matrix:mega2560_fast_release` | `arduino-generic/json` or `msgpack` |
| Arduino Uno             | `uno`              | `avr-board-matrix:uno_release`           | `arduino-generic/json`              |
| Arduino Nano ATmega328P | `nanoatmega328`    | `avr-board-matrix:nano_release`          | `arduino-generic/json`              |

For a new board, start with `preset = "arduino-generic/json"`, numeric Arduino pins and
`fast_io = false`. Enable faster codecs or direct-port I/O only after a release build
and static analysis are clean.

## Hardware Integration

The public Controllino path assumes a straightforward field model:

- wall buttons connect controller input pins to the controller supply voltage
- indicator outputs normally drive low-voltage LEDs or illuminated button panels
- relay outputs switch the attached loads within the limits of the board and
  installation
- the controller-to-bridge link uses a hardware serial port

On Controllino Maxi, the bridge link usually uses `Serial2`. A Controllino uses 5 V
logic while an ESP32 uses 3.3 V logic, so the serial path needs a proper logic level
shifter between controller `TX/RX` and ESP32 `RX/TX`.

For the full panel-level layout, power topology and serviceability notes, read the
[Labo Smart Home hardware overview](https://github.com/labodj/labo-smart-home/blob/main/HARDWARE_OVERVIEW.md).

## Runtime Shape

```text
+------------+    serial    +------------+    MQTT    +--------------+
| lsh-core   | <----------> | lsh-bridge | <--------> | coordinator  |
| controller |              | ESP32      |            | or Node-RED  |
+------------+              +------------+            +--------------+
```

Important invariants:

- topology is static between controller boots
- generated resource counts define the exact actuator/button/indicator capacity
- `lsh-core` sends `BOOT` after configuration so the bridge can resync details and state
- JSON serial uses newline-delimited frames
- MsgPack serial uses delimiter-and-escape framing
- the protocol assumes a trusted environment

For the full public runtime profile, read the
[LSH reference stack](https://github.com/labodj/labo-smart-home/blob/main/REFERENCE_STACK.md).

## Network Clicks and Fallback

Long and super-long clicks can ask the coordinator to perform distributed work. If the
network path does not complete in time, `lsh-core` applies the configured fallback.

```toml
long = { network = true, fallback = "local", targets = ["ceiling"], action = "on" }
super_long = { network = true, fallback = "do_nothing" }
```

Use `fallback = "local"` when the button needs to act on the local controller too. Use
`fallback = "do_nothing"` for actions that only make sense when the distributed runtime
is available.

## Documentation

- [DOCS.md](https://github.com/labodj/lsh-core/blob/main/DOCS.md): repository
  documentation map
- [docs/static-toml-config.md](https://github.com/labodj/lsh-core/blob/main/docs/static-toml-config.md):
  schema v2 reference
- [docs/cookbook.md](https://github.com/labodj/lsh-core/blob/main/docs/cookbook.md):
  configuration recipes
- [docs/feature-flags.md](https://github.com/labodj/lsh-core/blob/main/docs/feature-flags.md):
  compile-time tuning knobs
- [Doxygen API reference](https://labodj.github.io/lsh-core/): class and method
  documentation for the latest tagged release

The hosted API reference tracks the latest tagged release. The `main` branch may
describe newer work that has not been tagged yet.

## Building and Uploading

From a consumer PlatformIO project, use the normal environment names from that project:

```bash
platformio run -e kitchen_release
platformio run -e kitchen_debug --target upload
```

From a checkout of this repository, use the example paths shown in
[First Build](#first-build).

## PlatformIO Registry Releases

Maintainers publish the library as a PlatformIO package only after the package smoke
test has passed. That test packs the current checkout, verifies that the TOML generator
and schema are present, rejects local cache/build directories and builds a temporary
consumer project from the packed archive.

Manual release commands:

```bash
platformio pkg pack
platformio account login
platformio pkg publish --owner labodj --type library --no-interactive
```

After a Registry release, consumers can pin the Registry package:

```ini
lib_deps = labodj/lsh-core @ ^3.2.0
```
