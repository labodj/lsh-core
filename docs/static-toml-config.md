# Static TOML Configuration

`lsh-core` uses TOML as the public device configuration format. Users describe
devices with names, pins, IDs and click actions; the generator validates that
profile and emits the optimized C++ headers consumed by the library.

Generated headers are an implementation detail. Edit the TOML, regenerate, and
compile.

## Mental Model

A device profile has two parts:

- human input: `lsh_devices.toml`
- generated output: C++ headers under `include/`

For most projects, the TOML file is the file people maintain. It contains
names, public IDs, pins, click behavior, serial choices and compile-time
defines. The generated C++ contains dense indexes, lookup branches, static
payload bytes and registration code tailored to that profile.

This split is intentional. It keeps configuration readable while still allowing
the compiled firmware to be small and predictable on 8-bit AVR targets.

## Quick Start

Create `lsh_devices.toml` in the consumer project:

```toml
[generator]
output_dir = "include"
config_dir = "lsh_configs"
user_config_header = "lsh_user_config.hpp"

[common]
hardware_include = "Controllino.h"
debug_serial = "Serial"
com_serial = "Serial2"

[common.defines]
CONFIG_MSG_PACK = true
CONFIG_USE_FAST_CLICKABLES = true
CONFIG_USE_FAST_ACTUATORS = true
CONFIG_USE_FAST_INDICATORS = true

[devices.kitchen]
name = "kitchen"

[[devices.kitchen.actuators]]
name = "ceiling"
id = 10
pin = "CONTROLLINO_R0"
auto_off = "30m"

[[devices.kitchen.clickables]]
name = "door_button"
id = 42
pin = "CONTROLLINO_A0"
short = ["ceiling"]
long = { targets = ["ceiling"], type = "off_only", time = "900ms" }

[[devices.kitchen.indicators]]
name = "ceiling_led"
pin = "CONTROLLINO_D0"
actuators = ["ceiling"]
```

Add the PlatformIO pre-build hook:

```ini
[common_base]
extra_scripts = pre:path/to/lsh-core/tools/platformio_lsh_static_config.py
custom_lsh_config = lsh_devices.toml
build_src_filter = +<*> -<configs/>

[env:Kitchen_release]
extends = common_release
custom_lsh_device = kitchen
build_src_filter = ${common_base.build_src_filter}
```

The hook validates the TOML, writes generated headers under `output_dir`, adds
the selected `LSH_BUILD_*` macro, appends the generated include path, and applies
the merged TOML build defines.

`extra_scripts` must point to a real `lsh-core` checkout. The public example
uses a local symlink dependency; a consumer project can use an adjacent checkout
or a submodule when it wants a fixed release.

The selected device comes from PlatformIO:

```ini
[env:Kitchen_release]
custom_lsh_device = kitchen
```

Device keys may be lowercase, mixed with underscores, and independent from the
runtime device name sent on the wire. Public actuator and clickable IDs may be
sparse; users do not need to maintain lookup tables by hand.

For a deliberately exhaustive syntax catalog, see
`examples/all-options-toml/lsh_devices.toml`. It contains every currently
accepted field, duration form, click-action form, define value type and indicator
mode. It is validated by CI by generating headers in a temporary directory.

Manual generation is useful in CI or non-PlatformIO builds:

```bash
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --check
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --list-devices
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --device kitchen
python3 tools/generate_lsh_static_config.py path/to/lsh_devices.toml --print-platformio-defines kitchen
```

Python 3.11+ is preferred because it includes `tomllib`. Older Python versions
need the small `tomli` package.

## Recommended First Profile

For a first controller, keep the profile deliberately small:

```toml
[common]
hardware_include = "Controllino.h"
debug_serial = "Serial"
com_serial = "Serial2"

[common.defines]
CONFIG_MSG_PACK = true
CONFIG_USE_FAST_CLICKABLES = true
CONFIG_USE_FAST_ACTUATORS = true
CONFIG_USE_FAST_INDICATORS = true

[devices.first_panel]
name = "first_panel"

[[devices.first_panel.actuators]]
name = "relay_1"
id = 1
pin = "CONTROLLINO_R0"

[[devices.first_panel.clickables]]
name = "button_1"
id = 1
pin = "CONTROLLINO_A0"
short = ["relay_1"]
```

Add indicators, long-clicks, auto-off timers and network-click behavior only
after this basic path builds and behaves as expected. That keeps first failures
easy to diagnose.

## Files To Commit

For library examples and reproducible firmware projects, commit:

- `lsh_devices.toml`
- generated headers under `include/`
- the PlatformIO configuration that selects `custom_lsh_device`

For experiments where the build environment always runs the generator, generated
headers can be treated as build artifacts. The public examples commit them so a
reader can inspect the produced firmware profile without running tools first.

## Generator Section

`[generator]` controls where generated files are written.

| Field                | Type   | Default                 | Meaning                                                                                  |
| -------------------- | ------ | ----------------------- | ---------------------------------------------------------------------------------------- |
| `output_dir`         | string | `"include"`             | Relative output directory under the TOML file directory. It must not escape the project. |
| `config_dir`         | string | `"lsh_configs"`         | Generated subdirectory inside `output_dir`. Must be one path component.                  |
| `user_config_header` | string | `"lsh_user_config.hpp"` | Generated router header included by `lsh-core`. Must be one file name.                   |

Do not point `output_dir`, `config_dir`, `config_include` or
`static_config_include` outside the generated include directory.

## Common Section

`[common]` supplies defaults shared by all devices.

| Field              | Type             | Default                                   | Meaning                                                                                                            |
| ------------------ | ---------------- | ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| `hardware_include` | string           | required unless every device overrides it | Board header. `"Controllino.h"` becomes `<Controllino.h>`. Values already starting with `<` or `"` are kept as-is. |
| `debug_serial`     | string           | `"Serial"`                                | Debug serial object. `Serial` becomes `&Serial`.                                                                   |
| `com_serial`       | string           | `"Serial"`                                | Controller-to-bridge serial object. `Serial2` becomes `&Serial2`.                                                  |
| `raw_build_flags`  | array of strings | `[]`                                      | Raw PlatformIO build flags appended for every device.                                                              |

`[common.defines]` is merged into every device define table.

## Device Section

Each `[devices.<key>]` declares one buildable device. The key is also the default
selector used by PlatformIO and CLI commands.

| Field                   | Type             | Default                                | Meaning                                                 |
| ----------------------- | ---------------- | -------------------------------------- | ------------------------------------------------------- |
| `name`                  | string           | device key                             | Runtime device name sent on the wire.                   |
| `build_macro`           | string           | `LSH_BUILD_<KEY>`                      | Preprocessor macro generated for this profile.          |
| `hardware_include`      | string           | `[common].hardware_include`            | Device-specific board header.                           |
| `debug_serial`          | string           | `[common].debug_serial`                | Device-specific debug serial.                           |
| `com_serial`            | string           | `[common].com_serial`                  | Device-specific bridge serial.                          |
| `config_include`        | string           | `<config_dir>/<key>_config.hpp`        | Generated constants header path.                        |
| `static_config_include` | string           | `<config_dir>/<key>_static_config.hpp` | Generated static profile header path.                   |
| `disable_rtc`           | bool             | `false`                                | Emits `disableRtc()` during configuration.              |
| `disable_eth`           | bool             | `false`                                | Emits `disableEth()` during configuration.              |
| `raw_build_flags`       | array of strings | `[]`                                   | Raw PlatformIO build flags appended after common flags. |

`[devices.<key>.defines]` overrides `[common.defines]`. Set a common define to
`false` in a device to remove it for that device.

## Actuators

Declare actuators with `[[devices.<key>.actuators]]`.

| Field           | Type                  | Required | Meaning                                                                       |
| --------------- | --------------------- | -------- | ----------------------------------------------------------------------------- |
| `name`          | C++ identifier string | yes      | Local name used by clickables and indicators.                                 |
| `id`            | integer `1..255`      | yes      | Public wire ID. IDs may be sparse.                                            |
| `pin`           | string                | yes      | Compile-time Arduino pin expression, for example `"CONTROLLINO_R0"` or `"6"`. |
| `default_state` | bool                  | no       | Construct the actuator ON by default.                                         |
| `protected`     | bool                  | no       | Exclude the actuator from global super-long OFF.                              |
| `auto_off`      | duration              | no       | Auto-off timer, for example `"10m"`.                                          |
| `auto_off_ms`   | duration              | no       | Same timer expressed as milliseconds.                                         |

Durations accept integer milliseconds or strings ending in `ms`, `s`, `m`, `h`.

## Clickables

Declare buttons and inputs with `[[devices.<key>.clickables]]`.

| Field        | Type                  | Required           | Meaning                              |
| ------------ | --------------------- | ------------------ | ------------------------------------ |
| `name`       | C++ identifier string | yes                | Local button name.                   |
| `id`         | integer `1..255`      | yes                | Public wire ID. IDs may be sparse.   |
| `pin`        | string                | yes                | Compile-time Arduino pin expression. |
| `short`      | action                | yes for normal use | Short-click behavior.                |
| `long`       | action                | no                 | Long-click behavior.                 |
| `super_long` | action                | no                 | Super-long-click behavior.           |

Every clickable must have at least one enabled action.

### Short Click

Accepted forms:

```toml
short = ["relay_a", "relay_b"]
short = { targets = ["relay_a"] }
short = { enabled = true, targets = ["relay_a"] }
short = false
```

`short = true` is rejected because it is ambiguous.

### Long Click

Accepted forms:

```toml
long = ["relay_a"]
long = { targets = ["relay_a", "relay_b"] }
long = { actuators = ["relay_a"] }
long = { targets = ["relay_a"], type = "off_only", time = "900ms" }
long = { network = true, fallback = "do_nothing" }
long = { enabled = false }
```

Supported `type` values:

- `normal`
- `on_only` or `on-only`
- `off_only` or `off-only`

A long-click action must have at least one local target or `network = true`.

### Super-Long Click

Accepted forms:

```toml
super_long = true
super_long = { type = "normal" }
super_long = { type = "selective", targets = ["relay_a"] }
super_long = { type = "selective", actuators = ["relay_a"] }
super_long = { type = "selective" }
super_long = { network = true, fallback = "do_nothing" }
super_long = { enabled = false }
```

Supported `type` values:

- `normal`: global OFF for all unprotected actuators.
- `selective`: only the listed actuators are affected.

`super_long = { type = "selective" }` is an explicit no-op. It is supported for
legacy profiles that used selective super-long clicks to suppress the global OFF
behavior without attaching secondary actuators.

### Network Clicks

Both long and super-long actions support:

| Field      | Type   | Meaning                                             |
| ---------- | ------ | --------------------------------------------------- |
| `network`  | bool   | Send this click to `lsh-bridge` / `lsh-logic`.      |
| `fallback` | string | Local behavior if the network path cannot complete. |

Supported fallbacks:

- `local`, `local_fallback` or `local-fallback`
- `do_nothing` or `do-nothing`

If any action sets `network = true`, the generated profile keeps the network
click pool enabled and sizes it exactly. If no action uses network clicks, the
network-click subsystem is compiled out for that device.

## Indicators

Declare indicators with `[[devices.<key>.indicators]]`.

| Field                    | Type                    | Required | Meaning                                       |
| ------------------------ | ----------------------- | -------- | --------------------------------------------- |
| `name`                   | C++ identifier string   | yes      | Local indicator name.                         |
| `pin`                    | string                  | yes      | Compile-time Arduino pin expression.          |
| `actuators` or `targets` | array of actuator names | yes      | Actuators watched by the indicator.           |
| `mode`                   | string                  | no       | `any`, `all` or `majority`; default is `any`. |

Indicators must reference at least one actuator.

## Defines

Defines can be placed in `[common.defines]` or `[devices.<key>.defines]`.

```toml
[common.defines]
CONFIG_MSG_PACK = true
CONFIG_USE_FAST_ACTUATORS = true
CONFIG_DEBUG_SERIAL_BAUD = 500000

[devices.garage.defines]
CONFIG_MSG_PACK = false
CONFIG_CLICKABLE_DEBOUNCE_TIME_MS = 30
CONFIG_COM_SERIAL_FLUSH_AFTER_SEND = 1
```

Value rules:

- `true`: emit a macro without value, for example `CONFIG_MSG_PACK`.
- `false`: do not emit this macro; useful to disable a common define.
- integer: emit `NAME=value`.
- string: emit `NAME=value` exactly as written.

String define values are raw preprocessor text. Use PlatformIO `build_flags`
when a value needs shell or INI interpolation, or when an include-like value
needs careful quoting.

## Raw Build Flags

Use `raw_build_flags` for flags that are not simple preprocessor defines:

```toml
[common]
raw_build_flags = ["-flto=auto"]

[devices.garage]
raw_build_flags = ["-Wl,--gc-sections"]
```

The PlatformIO hook appends common raw flags first and device raw flags after.
For flags that must replace PlatformIO defaults, keep using `build_unflags` in
`platformio.ini` and then add the desired flag through `build_flags` or
`raw_build_flags`.

## Supported lsh-core Defines

These are the user-facing defines currently intended for TOML define tables or
PlatformIO `build_flags`.

For AVR static profiles, the recommended default priority is runtime speed
first, SRAM second and flash third. Put `CONFIG_MSG_PACK = true` and the three
`CONFIG_USE_FAST_* = true` flags in `[common.defines]` unless a specific
installation needs JSON compatibility, lower flash use or slower portable I/O.

| Define                                            | Default                        | Meaning                                                                                                                   |
| ------------------------------------------------- | ------------------------------ | ------------------------------------------------------------------------------------------------------------------------- |
| `CONFIG_MSG_PACK`                                 | undefined                      | Use framed MessagePack instead of newline-delimited JSON. Recommended when runtime speed and SRAM matter more than flash. |
| `CONFIG_USE_FAST_CLICKABLES`                      | undefined                      | Use direct port reads for clickables when supported. Recommended for AVR static profiles.                                 |
| `CONFIG_USE_FAST_ACTUATORS`                       | undefined                      | Use direct port writes for actuators when supported. Recommended for AVR static profiles.                                 |
| `CONFIG_USE_FAST_INDICATORS`                      | undefined                      | Use direct port writes for indicators when supported. Recommended for AVR static profiles.                                |
| `CONFIG_ACTUATOR_DEBOUNCE_TIME_MS`                | `100U`                         | Minimum actuator switch interval.                                                                                         |
| `CONFIG_CLICKABLE_DEBOUNCE_TIME_MS`               | `20U`                          | Button debounce time.                                                                                                     |
| `CONFIG_CLICKABLE_SCAN_INTERVAL_MS`               | `1U`                           | Minimum elapsed time between local input scan passes.                                                                     |
| `CONFIG_CLICKABLE_LONG_CLICK_TIME_MS`             | `400U`                         | Default long-click threshold.                                                                                             |
| `CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS`       | `1000U`                        | Default super-long-click threshold.                                                                                       |
| `CONFIG_LCNB_TIMEOUT_MS`                          | `1000U`                        | Network-click ACK timeout.                                                                                                |
| `CONFIG_PING_INTERVAL_MS`                         | `10000U`                       | Bridge ping interval.                                                                                                     |
| `CONFIG_CONNECTION_TIMEOUT_MS`                    | ping interval + `200U`         | Link liveness timeout.                                                                                                    |
| `CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS`            | `250U`                         | BOOT retry interval while bridge is not ready.                                                                            |
| `CONFIG_BRIDGE_AWAIT_STATE_TIMEOUT_MS`            | `1500U`                        | Timeout while waiting for bridge state request.                                                                           |
| `CONFIG_DEBUG_SERIAL_BAUD`                        | `115200U`                      | Debug UART speed in debug builds.                                                                                         |
| `CONFIG_COM_SERIAL_BAUD`                          | `250000U`                      | Controller-to-bridge UART speed.                                                                                          |
| `CONFIG_COM_SERIAL_TIMEOUT_MS`                    | `5U`                           | Compatibility default for MsgPack idle timeout.                                                                           |
| `CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS` | `CONFIG_COM_SERIAL_TIMEOUT_MS` | Timeout used to discard one incomplete MsgPack frame.                                                                     |
| `CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP`      | `4U`                           | Max fully dispatched bridge payloads per loop iteration.                                                                  |
| `CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP`         | mode-dependent                 | Max raw UART bytes drained per loop iteration.                                                                            |
| `CONFIG_COM_SERIAL_FLUSH_AFTER_SEND`              | debug: `1`, release: `0`       | Force `Serial.flush()` after bridge sends.                                                                                |
| `CONFIG_DELAY_AFTER_RECEIVE_MS`                   | `50U`                          | Quiet window after received bridge state changes.                                                                         |
| `CONFIG_NETWORK_CLICK_CHECK_INTERVAL_MS`          | `50U`                          | Pending network-click timeout polling interval.                                                                           |
| `CONFIG_ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS`     | `1000U`                        | Auto-off scan interval.                                                                                                   |
| `CONFIG_LSH_BENCH`                                | undefined                      | Enable developer loop benchmark.                                                                                          |
| `CONFIG_BENCH_ITERATIONS`                         | `1000000U`                     | Benchmark loop iteration count.                                                                                           |
| `LSH_ENABLE_AGGRESSIVE_CONSTEXPR_CTORS`           | auto                           | Force aggressive constexpr constructors when supported.                                                                   |
| `LSH_DISABLE_AGGRESSIVE_CONSTEXPR_CTORS`          | undefined                      | Disable aggressive constexpr constructors.                                                                                |
| `LSH_ETL_PROFILE_OVERRIDE_HEADER`                 | undefined                      | Include a consumer-provided ETL profile override header. Prefer `platformio.ini` for quoted include values.               |

Static resource macros such as `LSH_STATIC_CONFIG_ACTUATORS`,
`LSH_STATIC_CONFIG_LONG_CLICK_ACTUATOR_LINKS`, `CONFIG_MAX_ACTUATORS` and
`CONFIG_USE_COMPACT_ACTUATOR_SWITCH_TIMES` are generated or derived internally.
Do not write them by hand in TOML.

Network-click support is inferred from click actions. If at least one enabled
long or super-long action has `network = true`, the generated profile sizes and
enables the active network-click pool. If no action uses the bridge, the runtime
path is compiled out.

Compact actuator switch-time storage is automatic. It is enabled only when the
profile has auto-off actuators and `CONFIG_ACTUATOR_DEBOUNCE_TIME_MS` is
explicitly `0`; otherwise each actuator keeps the timestamp required to preserve
debounce and auto-off semantics exactly.

PlatformIO-only flags such as `NDEBUG`, `LSH_DEBUG`, `SERIAL_RX_BUFFER_SIZE`,
`SERIAL_TX_BUFFER_SIZE`, compiler optimization flags and linker flags can stay
in `platformio.ini`. They may also be placed in `raw_build_flags` when appending
is enough.

## Validation

The generator fails before compilation when it finds:

- malformed TOML;
- missing required tables or fields;
- invalid C++ identifiers or preprocessor macro names;
- duplicate names or IDs;
- IDs, counts or timing overrides outside generated field widths;
- unknown actuator references;
- duplicated targets in one action;
- disabled actions with active targets/options;
- enabled long clicks with no local target and no network action;
- indicators with no targets;
- removed internal defines such as `LSH_NETWORK_CLICKS` or `LSH_COMPACT_ACTUATOR_SWITCH_TIMES`;
- unsafe C++ pin or serial expressions;
- generated paths that escape the output directory.

This keeps configuration mistakes close to the TOML and avoids runtime defensive
lookup tables on AVR.

## Migration From Older Hand-Written Profiles

Older consumers used one C++ file per device and selected it with
`build_src_filter` plus an `LSH_BUILD_*` define. New consumers should move that
information into TOML:

- actuator constructors become `[[devices.<key>.actuators]]`
- clickable constructors become `[[devices.<key>.clickables]]`
- indicator constructors become `[[devices.<key>.indicators]]`
- manual local target lists become action `targets`
- per-device build defines move to `[devices.<key>.defines]`
- shared lsh-core defines move to `[common.defines]`

After the migration, remove legacy `src/configs/*.cpp` files from the build and
let `platformio_lsh_static_config.py` inject the selected `LSH_BUILD_*` macro.
Keep compiler, linker, upload and board-specific PlatformIO settings in
`platformio.ini`.

## Generated Code Strategy

The generated profile avoids SRAM tables for static facts:

- resource capacities are preprocessor constants;
- IDs and reverse-ID lookups are compact branch/range accessors;
- auto-off timers are branch-grouped by equal duration;
- long/super-long click types and per-click thresholds are generated accessors;
- click and indicator links are generated as dense-index accessors, with common
  single-link and range cases compressed into arithmetic branches;
- indicator modes are generated accessors;
- DEVICE_DETAILS JSON and serial-framed MsgPack payloads are pre-serialized at
  generation time and stored in flash on AVR targets;
- network-click pools are sized exactly for the profile and are compiled out
  when no `network = true` action exists.
- compact actuator switch-time storage is selected automatically when actuator
  debounce is disabled and only auto-off actuators need switch timestamps.

Keyword policy:

- generated constants that must size arrays or select code paths are emitted as
  preprocessor constants and imported as internal `constexpr` values by
  `src/internal/user_config_bridge.hpp`;
- AVR payload byte arrays use `const ... PROGMEM`, because flash placement is the
  relevant storage decision on this target;
- generated lookup accessors are emitted in one implementation pass instead of
  being header-only `constexpr` functions. That keeps one copy of the branch
  logic in the firmware and avoids turning implementation detail into public
  inline code;
- generated accessors are declared `[[nodiscard]]` and `noexcept` because they
  are deterministic embedded helpers and their return values carry the whole
  result;
- generated device objects are not `constexpr`: constructing them participates
  in setup-time hardware binding and, depending on the selected fast-I/O support,
  may still need normal object construction before `setup()` finalization.

Commit generated headers if the target build environment will not run the
generator. Otherwise, treat `lsh_devices.toml` as the source of truth.
