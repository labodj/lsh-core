# Static TOML Configuration

`lsh-core` uses TOML as the public device configuration format. A user describes
controllers, relays, buttons, indicators and click behavior in one declarative
file; the generator validates it and emits optimized C++ for the selected
firmware profile.

Generated headers are implementation detail. Edit `lsh_devices.toml`,
regenerate, and build. Commit `lsh_devices.lock.toml` with the TOML profile
whenever IDs are auto-assigned, because it preserves the public wire IDs across
future edits.

## Mental Model

A profile has two layers:

- public input: `lsh_devices.toml`
- generated output: C++ headers under the configured include directory

The public schema is intentionally friendly: resources are named TOML tables,
buttons reference actuators by name, Controllino presets accept short pin names
such as `R0` and `A0`, and common behavior uses semantic fields instead of raw
preprocessor defines.

The generated C++ is intentionally not friendly: it contains dense indexes,
specialized switch/range lookups, static payload bytes, direct click action
bodies and exact resource counts tailored to one device. This split keeps the
configuration easy to read while still giving the AVR firmware compile-time
topology.

## Quick Start

Install the library from the PlatformIO Registry:

```ini
[env:Kitchen_release]
platform = atmelavr
framework = arduino
board = controllino_maxi
lib_deps = labodj/lsh-core @ ^3.0.4
```

Then create `lsh_devices.toml` in the consumer project with the guided
scaffold. Run `platformio pkg install` once first if the package has not yet
been downloaded into `.pio/libdeps`:

```bash
platformio pkg install
python3 .pio/libdeps/Kitchen_release/lsh-core/tools/generate_lsh_static_config.py \
  --init-config lsh_devices.toml \
  --preset controllino-maxi/fast-msgpack \
  --device-key kitchen \
  --relays 4 \
  --buttons 4 \
  --indicators 1
```

The command also writes `.vscode/lsh_devices.schema.json`, a project-specific
autocomplete schema that knows the actuator, group and scene names in your TOML.

A compact hand-written profile looks like this:

```toml
#:schema ./.vscode/lsh_devices.schema.json

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

Add the PlatformIO pre-build hook:

```ini
[env:Kitchen_release]
extra_scripts = pre:.pio/libdeps/Kitchen_release/lsh-core/tools/platformio_lsh_static_config.py
custom_lsh_config = lsh_devices.toml
custom_lsh_device = kitchen
build_src_filter = +<*> -<configs/>
```

The hook validates the TOML, writes generated headers, adds the selected
`LSH_BUILD_*` macro, appends the generated include path, and applies the merged
semantic options, expert defines and raw build flags.

For a local checkout or submodule, keep the same TOML but point the hook at that
checkout instead, for example
`extra_scripts = pre:../lsh-core/tools/platformio_lsh_static_config.py`.

Manual generation is useful in CI or non-PlatformIO builds:

```bash
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --check
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --list-devices
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --device kitchen
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --print-platformio-defines kitchen
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --doctor
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --format-config
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --check-format
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --write-vscode-schema
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --print-project-json-schema
python3 tools/generate_lsh_static_config.py --print-json-schema
python3 tools/generate_lsh_static_config.py --init-config path/to/lsh_devices.toml
```

Python 3.11+ is required by the project tooling.

## Editor Autocomplete

Use a schema file inside the same workspace as the consumer project. This keeps
Taplo-based VS Code extensions, including Even Better TOML, away from paths
outside the opened folder.

```bash
python3 path/to/lsh-core/tools/generate_lsh_static_config.py lsh_devices.toml --write-vscode-schema
```

Then keep the directive at the top of `lsh_devices.toml`:

```toml
#:schema ./.vscode/lsh_devices.schema.json
```

The generated schema is project-specific: it suggests the actual actuator,
group and scene names already present in the TOML file. Re-run the command
after adding or renaming resources.

## Presets

`preset` is the fastest adoption path. It selects safe defaults that can still
be overridden later.

| Preset                          | Board defaults                                 | Codec   | Fast I/O |
| ------------------------------- | ---------------------------------------------- | ------- | -------- |
| `controllino-maxi/fast-msgpack` | `Controllino.h`, `Serial2` bridge, pin aliases | MsgPack | on       |
| `controllino-maxi/fast-json`    | `Controllino.h`, `Serial2` bridge, pin aliases | JSON    | on       |
| `arduino-generic/msgpack`       | `Arduino.h`, `Serial` bridge                   | MsgPack | off      |
| `arduino-generic/json`          | `Arduino.h`, `Serial` bridge                   | JSON    | off      |

## AVR Board Matrix

Controllino is a supported board family, not a hard dependency. For non-
Controllino hardware, use the generic presets and keep the first profile
conservative:

```toml
schema_version = 2
preset = "arduino-generic/json"

[controller]
hardware_include = "Arduino.h"
debug_serial = "Serial"
bridge_serial = "Serial"

[features]
fast_io = false
```

The repository keeps a CI-backed smoke matrix in
`examples/avr-board-matrix`:

| Board / family          | PlatformIO board | Profile            | Fast I/O | Status                  |
| ----------------------- | ---------------- | ------------------ | -------- | ----------------------- |
| Arduino Mega 2560       | `megaatmega2560` | `mega2560_fast`    | on       | supported               |
| Arduino Uno             | `uno`            | `atmega328p_basic` | off      | supported, conservative |
| Arduino Nano ATmega328P | `nanoatmega328`  | `atmega328p_basic` | off      | supported, conservative |

The Controllino Maxi path is covered separately by
`examples/multi-device-project`, because that profile intentionally uses
Controllino-specific headers, aliases and optional board helpers.

Boards outside this matrix can still work when they provide the normal Arduino
AVR API and enough flash/RAM, but treat them as best effort until they build and
pass static analysis in your own CI.

Controllino pin aliases expand as follows:

```toml
pin = "R0"   # CONTROLLINO_R0
pin = "A0"   # CONTROLLINO_A0
pin = "D6"   # CONTROLLINO_D6
pin = "IN0"  # CONTROLLINO_IN0
```

Use `pin = "raw:MY_PIN_EXPR"` when a symbol intentionally looks like an alias
but must be passed through unchanged.

## Project Sections

### `schema_version`

Always set:

```toml
schema_version = 2
```

Schema v2 is the public, ergonomic dialect. Old generated internals are not part
of the user contract.

### `[generator]`

Optional. Defaults are good for most PlatformIO projects.

| Field                         | Default                          | Meaning                                               |
| ----------------------------- | -------------------------------- | ----------------------------------------------------- |
| `output_dir`                  | `"include"`                      | Generated output directory relative to the TOML file. |
| `config_dir`                  | `"lsh_configs"`                  | Generated subdirectory inside `output_dir`.           |
| `user_config_header`          | `"lsh_user_config.hpp"`          | Generated public router header.                       |
| `static_config_router_header` | `"lsh_static_config_router.hpp"` | Generated internal two-pass router header.            |
| `id_lock_file`                | `"lsh_devices.lock.toml"`        | Generated stable-ID lockfile beside the TOML config.  |

### `[controller]`

Project-wide hardware defaults.

| Field                     | Meaning                                                     |
| ------------------------- | ----------------------------------------------------------- |
| `hardware_include`        | Board header. Bare names become angle-bracket includes.     |
| `debug_serial`            | Debug serial object, for example `Serial`.                  |
| `bridge_serial`           | Controller-to-bridge serial object, for example `Serial2`.  |
| `disable_rtc`             | Emit Controllino RTC chip-select disable during setup.      |
| `disable_eth`             | Emit Controllino Ethernet chip-select disable during setup. |
| `controllino_pin_aliases` | Enable or disable `R0` / `A0` / `D0` / `IN0` expansion.     |

### `[features]`

Semantic feature switches. These are preferred over raw `CONFIG_*` defines.

| Field                         | Values                       | Meaning                                                         |
| ----------------------------- | ---------------------------- | --------------------------------------------------------------- |
| `codec`                       | `"msgpack"` or `"json"`      | Serial payload codec.                                           |
| `fast_io`                     | bool                         | Apply one fast-I/O policy to buttons, actuators and indicators. |
| `fast_buttons`                | bool                         | Override fast input reads only.                                 |
| `fast_actuators`              | bool                         | Override fast actuator writes only.                             |
| `fast_indicators`             | bool                         | Override fast indicator writes only.                            |
| `bench`                       | bool                         | Enable the developer loop benchmark.                            |
| `bench_iterations`            | integer                      | Benchmark loop count.                                           |
| `aggressive_constexpr_ctors`  | `true`, `false`, or `"auto"` | Constructor constexpr policy.                                   |
| `etl_profile_override_header` | string or `false`            | Optional consumer ETL profile override header.                  |

### `[timing]`

Durations accept integers in milliseconds or strings ending in `ms`, `s`, `m`
or `h`.

| Field                          | Meaning                                                       |
| ------------------------------ | ------------------------------------------------------------- |
| `actuator_debounce`            | Minimum interval between actuator switches. `0ms` is allowed. |
| `button_debounce`              | Button debounce threshold.                                    |
| `scan_interval`                | Minimum elapsed time between input scan passes.               |
| `long_click`                   | Default long-click threshold.                                 |
| `super_long_click`             | Default super-long-click threshold.                           |
| `network_click_timeout`        | Network-click ACK timeout.                                    |
| `ping_interval`                | Bridge ping interval.                                         |
| `connection_timeout`           | Bridge liveness timeout.                                      |
| `bridge_boot_retry`            | BOOT retry interval.                                          |
| `bridge_state_timeout`         | Timeout while waiting for bridge state request.               |
| `post_receive_delay`           | Quiet window after bridge-side state changes.                 |
| `network_click_check_interval` | Pending network-click polling interval.                       |
| `auto_off_check_interval`      | Auto-off scan interval.                                       |

`long_click` and `super_long_click` are also propagated into generated static
button scanner templates for actions that do not define their own `after` /
`time` value.

### `[serial]`

Serial transport tuning.

| Field                        | Meaning                                            |
| ---------------------------- | -------------------------------------------------- |
| `debug_baud`                 | Debug UART baud rate.                              |
| `bridge_baud`                | Controller-to-bridge UART baud rate.               |
| `timeout`                    | Compatibility serial timeout.                      |
| `msgpack_frame_idle_timeout` | Timeout used to discard incomplete MsgPack frames. |
| `max_rx_payloads_per_loop`   | Max complete bridge payloads dispatched per loop.  |
| `max_rx_bytes_per_loop`      | Max raw UART bytes drained per loop.               |
| `flush_after_send`           | Force serial flush after sends.                    |
| `rx_buffer_size`             | Emits `-D SERIAL_RX_BUFFER_SIZE=<value>`.          |
| `tx_buffer_size`             | Emits `-D SERIAL_TX_BUFFER_SIZE=<value>`.          |

### `[advanced]`

Expert-only escape hatch. Keep normal profiles on semantic fields.

```toml
[advanced]
build_flags = ["-flto=auto"]

[advanced.defines]
CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP = 64
```

Device sections also support `[devices.<key>.features]`,
`[devices.<key>.timing]`, `[devices.<key>.serial]` and
`[devices.<key>.advanced]`. Device values override project defaults.

## Devices

Each `[devices.<key>]` declares one buildable firmware profile. The key is the
PlatformIO selector used by `custom_lsh_device`.

```toml
[devices.kitchen]
name = "kitchen"
```

Common device fields:

| Field                     | Meaning                                                    |
| ------------------------- | ---------------------------------------------------------- |
| `name`                    | Runtime device name sent on the wire. Defaults to the key. |
| `build_macro`             | Expert override for the generated `LSH_BUILD_*` macro.     |
| `hardware_include`        | Device-specific board header.                              |
| `debug_serial`            | Device-specific debug serial object.                       |
| `bridge_serial`           | Device-specific bridge serial object.                      |
| `config_include`          | Expert generated constants header path.                    |
| `static_config_include`   | Expert generated static profile header path.               |
| `disable_rtc`             | Device override for Controllino RTC disable.               |
| `disable_eth`             | Device override for Controllino Ethernet disable.          |
| `controllino_pin_aliases` | Device override for pin alias expansion.                   |

## Actuators

Actuators are named subtables:

```toml
[devices.kitchen.actuators.ceiling]
pin = "R0"
auto_off = "30m"
```

| Field         | Required | Meaning                                                               |
| ------------- | -------- | --------------------------------------------------------------------- |
| `id`          | no       | Public wire ID. Omit to auto-assign a deterministic ID.               |
| `pin`         | yes      | Arduino pin expression or board alias.                                |
| `default`     | no       | Start ON.                                                             |
| `protected`   | no       | Exclude from global super-long OFF.                                   |
| `auto_off`    | no       | Auto-off duration.                                                    |
| `auto_off_ms` | no       | Auto-off duration in milliseconds.                                    |
| `pulse`       | no       | Momentary output duration, for example `300ms`.                       |
| `pulse_ms`    | no       | Momentary output duration in milliseconds.                            |
| `interlock`   | no       | Actuator or list of actuators to switch OFF before this one turns ON. |

When `id` is omitted, the generator writes `lsh_devices.lock.toml` and reuses
that locked value on future runs. Commit the lockfile with the TOML profile so
public wire IDs remain stable even when resources are reordered or inserted.

`pulse` is for hardware that must receive a short ON pulse, such as a strike,
bell or garage input. Any generated ON command, including serial commands from
the bridge, starts or restarts the pulse countdown. OFF cancels a pending pulse
and switches the output off. Use `auto_off` instead when the relay is a normal
latched output that should stay ON but have a guard timer.

`interlock` is resolved at generation time. The emitted setter turns listed
actuators OFF before turning the selected actuator ON, and the same rule is used
by local clicks, scenes, packed bridge state and direct serial commands.

## Groups and Scenes

Groups are local aliases for actuator lists. They are only TOML conveniences;
the generated code receives the expanded actuator indexes directly.

```toml
[devices.kitchen.groups.worktop]
targets = ["left_strip", "right_strip"]

[devices.kitchen.buttons.worktop]
pin = "A1"
short = { group = "worktop" }
long = { action = "off", group = "worktop" }
```

Scenes are deterministic steps. `off` runs first, then `on`, then `toggle`, so
the generated code stays predictable even when a scene mixes operations.

```toml
[devices.kitchen.scenes.cooking]
off = "ambient"
on = ["worktop", "ceiling"]
toggle = "extractor"

[devices.kitchen.buttons.scene]
pin = "A2"
short = { scene = "cooking" }
```

Scene entries may name actuators or groups. If an actuator and a group share the
same name, the scene is rejected as ambiguous; explicit `group = ...` action
fields do not have that ambiguity.

## Buttons

Buttons use `[devices.<key>.buttons.<name>]`.
Button, actuator and indicator names are independent: the same logical name may
be reused in each family, and actuator references still resolve only against
`actuators`.

```toml
[devices.kitchen.buttons.door]
pin = "A0"
short = "ceiling"
long = { after = "900ms", action = "off", target = "ceiling" }
super_long = { action = "all_off" }
```

| Field        | Required   | Meaning                                |
| ------------ | ---------- | -------------------------------------- |
| `id`         | no         | Public wire ID. Omit to auto-assign.   |
| `pin`        | yes        | Arduino pin expression or board alias. |
| `short`      | normal use | Short-click behavior.                  |
| `long`       | no         | Long-click behavior.                   |
| `super_long` | no         | Super-long-click behavior.             |

Target shorthands:

```toml
short = "relay_a"
short = ["relay_a", "relay_b"]
short = { target = "relay_a" }
short = { targets = ["relay_a", "relay_b"] }
short = { group = "room" }
short = { scene = "evening" }
short = false
```

Long-click actions:

```toml
long = "relay_a"                                      # toggle target
long = ["relay_a", "relay_b"]                         # toggle targets
long = { action = "on", targets = ["relay_a"] }
long = { action = "off", target = "relay_a", after = "900ms" }
long = { action = "off", group = "room" }
long = { scene = "evening", after = "900ms" }
long = { network = true, fallback = "do_nothing" }
long = { enabled = false }
```

Supported long actions: `toggle`, `on`, `off`.

Super-long actions:

```toml
super_long = { action = "all_off" }
super_long = { action = "off", targets = ["relay_a", "relay_b"] }
super_long = { action = "off", group = "room" }
super_long = { scene = "shutdown" }
super_long = { network = true, fallback = "do_nothing" }
super_long = { enabled = false }
```

Supported super-long actions:

- `all_off`: switch off every unprotected actuator.
- `off`: switch off only the listed targets.

Network-click options on `long` and `super_long`:

| Field      | Values                        | Meaning                                       |
| ---------- | ----------------------------- | --------------------------------------------- |
| `network`  | bool                          | Send the click to `lsh-bridge` / `lsh-logic`. |
| `fallback` | `local`, `do_nothing`, `none` | Behavior if the network path fails.           |

If no enabled action uses `network = true`, the generated profile compiles out
the network-click runtime for that device.

## Indicators

Indicators are named subtables:

```toml
[devices.kitchen.indicators.ceiling_led]
pin = "D0"
when = "ceiling"
```

Accepted `when` forms:

```toml
when = "relay_a"
when = ["relay_a", "relay_b"]
when = { any = ["relay_a", "relay_b"] }
when = { all = ["relay_a", "relay_b"] }
when = { majority = ["relay_a", "relay_b", "relay_c"] }
```

Use `when` instead of manually pairing target lists with a separate mode field.

## Public Tooling

The generator includes adoption helpers that do not change runtime behavior:

- `--print-json-schema` prints the editor schema also committed as
  `docs/lsh_devices.schema.json`.
- `--print-project-json-schema` prints the same schema specialized with the
  device, actuator, group and scene names from one TOML file.
- `--write-vscode-schema [PATH]` writes that project-specific schema. Without a
  path, it uses `.vscode/lsh_devices.schema.json` beside the TOML file, matching
  the scaffold's `#:schema` directive.
- `--init-config PATH` writes a guided starter profile. Use `--preset`,
  `--device-key`, `--device-name`, `--relays`, `--buttons`, `--indicators` and
  `--force` to tune the generated file.
- `--doctor` prints non-fatal advice, such as automatic IDs without a committed
  lockfile or raw defines that have semantic schema v2 fields.
- `--format-config` rewrites TOML into deterministic table order. It preserves
  the `#:schema` editor directive; the canonical output is otherwise
  comment-free.
- `--check-format` fails when TOML formatting is stale.
- `python3 tools/migrate_lsh_config.py old.toml --output lsh_devices.toml`
  performs a one-shot conversion from old TOML to schema v2. The main generator
  itself does not accept legacy profiles.

Use the generic schema when documenting the public format. Use the
project-specific schema in consumer repositories, because it gives autocomplete
for the names that matter while still allowing new resources to be added before
the schema is refreshed.

## Validation

The generator fails before compilation when it finds:

- malformed TOML;
- unsupported schema v2 fields, which catches typos early;
- invalid C++ identifiers or preprocessor macro names;
- duplicate names or IDs;
- more than 255 actuators, buttons or indicators in one profile;
- IDs or timing overrides outside generated field widths;
- unknown actuator references;
- duplicated targets in one action;
- disabled actions that still contain active options;
- scenes that assign the same actuator to conflicting operations;
- pulse actuators combined with `auto_off`;
- interlock declarations that reference unknown actuators or themselves;
- enabled long clicks with no local target and no network action;
- super-long selective actions that target protected actuators;
- super-long thresholds that are not greater than long-click thresholds;
- indicators with no targets;
- removed internal defines such as `LSH_NETWORK_CLICKS` or `LSH_COMPACT_ACTUATOR_SWITCH_TIMES`;
- unsafe C++ pin or serial expressions;
- generated paths that escape the output directory.

This keeps configuration mistakes close to the TOML and avoids defensive
runtime lookup tables on AVR.

## Generated Code Strategy

The generated profile avoids SRAM tables for static facts:

- resource capacities are preprocessor constants;
- device name and serial objects are generated as C++ `constexpr` values;
- IDs and reverse-ID lookups are compact branch/range accessors;
- auto-off timers are branch-grouped by equal duration;
- button objects store only dynamic FSM state;
- generated actuator wrappers centralize pulse and interlock behavior, so local
  clicks and bridge commands cannot drift semantically;
- generated action paths pass actuator indexes as compile-time constants;
- click actions, network fallback routing, scanning, indicator refreshes and
  auto-off sweeps are emitted as topology-specialized code;
- DEVICE_DETAILS JSON and serial-framed MsgPack payloads are pre-serialized at
  generation time and stored in flash on AVR targets;
- network-click pools are sized exactly and compiled out when unused;
- compact actuator switch-time storage is selected automatically when actuator
  debounce is disabled and only auto-off actuators need switch timestamps.

Commit generated headers if the target build environment will not run the
generator. Otherwise, treat `lsh_devices.toml` as the source of truth.
