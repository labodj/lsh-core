# lsh-core Configuration Cookbook

This cookbook collects production-shaped `lsh_devices.toml` recipes for schema
v2. Each recipe is declarative and generator-friendly: the TOML remains easy to
read, while the emitted C++ stays static and specialized for the selected
device.

For a complete file that combines several recipes, see
[`examples/cookbook/lsh_devices.toml`](../examples/cookbook/lsh_devices.toml).

## Create a New Device File

Use the scaffold for a valid starting point with local editor autocomplete:

```bash
python3 tools/generate_lsh_static_config.py --init-config lsh_devices.toml \
  --preset controllino-maxi/fast-msgpack \
  --device-key kitchen \
  --device-name kitchen \
  --relays 4 \
  --buttons 4 \
  --indicators 1
```

The scaffold writes:

- `lsh_devices.toml`
- `.vscode/lsh_devices.schema.json`

Keep this line at the top of the TOML file:

```toml
#:schema ./.vscode/lsh_devices.schema.json
```

After renaming actuators, groups or scenes, refresh the project-specific schema:

```bash
python3 tools/generate_lsh_static_config.py lsh_devices.toml --write-vscode-schema
```

## One Relay, One Button, One Indicator

```toml
schema_version = 2
preset = "controllino-maxi/fast-msgpack"

[devices.kitchen]
name = "kitchen"

[devices.kitchen.actuators.ceiling]
pin = "R0"

[devices.kitchen.buttons.door]
pin = "A0"
short = "ceiling"

[devices.kitchen.indicators.ceiling_led]
pin = "D0"
when = "ceiling"
```

This is the smallest useful local loop: a short press toggles the relay and the
indicator mirrors it.

## Auto-Off Guard

```toml
[devices.kitchen.actuators.worktop]
pin = "R1"
auto_off = "45m"
```

Use `auto_off` for normal latched outputs that may be left on accidentally. The
relay still behaves like a normal ON/OFF actuator; the generated runtime only
adds the guard timer.

## Momentary Pulse Output

```toml
[devices.kitchen.actuators.door_strike]
pin = "R2"
pulse = "300ms"

[devices.kitchen.buttons.strike]
pin = "A1"
short = "door_strike"
```

Use `pulse` for strikes, bells and inputs that must receive a short ON impulse.
Any generated ON path starts or restarts the pulse. OFF cancels it.

## Local Groups

```toml
[devices.kitchen.groups.main_lights]
targets = ["ceiling", "worktop", "ambient"]

[devices.kitchen.buttons.main]
pin = "A2"
short = { group = "main_lights" }
long = { action = "off", group = "main_lights" }
```

Groups are TOML aliases only. The generator expands them into direct actuator
operations, so they improve readability without adding runtime lookup tables.

## Scenes

```toml
[devices.kitchen.scenes.cooking]
off = "ambient"
on = ["ceiling", "worktop"]

[devices.kitchen.buttons.scene]
pin = "A3"
short = { scene = "cooking" }
```

Scene steps always run in this order: `off`, then `on`, then `toggle`. That
ordering keeps mixed scenes deterministic.

## Selective Super-Long Off

```toml
[devices.kitchen.buttons.bedtime]
pin = "A4"
super_long = { action = "off", group = "main_lights" }
```

Use this when a panel should shut down a known set of outputs but must not touch
the whole device.

## Global Super-Long Off With Protected Relays

```toml
[devices.kitchen.actuators.service]
pin = "R6"
protected = true

[devices.kitchen.buttons.door]
pin = "A0"
short = "ceiling"
super_long = { action = "all_off" }
```

`all_off` switches off every unprotected actuator. Mark service outputs,
technical relays or always-on circuits as `protected = true`.

## Network Click Without Local Fallback

```toml
[devices.kitchen.buttons.logic_only]
pin = "A5"
long = { network = true, fallback = "do_nothing" }
```

This sends the click to `lsh-bridge` / Node-RED and does nothing locally if the
network path fails. If a button must still act locally when the network is down,
combine `network = true`, `fallback = "local"` and local targets.

## Motor Direction Interlock

```toml
[devices.kitchen.actuators.blind_up]
pin = "R4"
interlock = "blind_down"

[devices.kitchen.actuators.blind_down]
pin = "R5"
interlock = "blind_up"
```

When one interlocked actuator turns ON, the generated setter turns its peer OFF
first. The rule is applied consistently to local clicks, scenes and bridge
commands.

## Indicator Modes

```toml
[devices.kitchen.indicators.any_light_led]
pin = "D1"
when = { any = ["ceiling", "worktop", "ambient"] }

[devices.kitchen.indicators.cooking_led]
pin = "D2"
when = { all = ["ceiling", "worktop"] }

[devices.kitchen.indicators.majority_led]
pin = "D3"
when = { majority = ["ceiling", "worktop", "ambient"] }
```

Indicators are computed directly from generated actuator references. Use the
mode that matches the panel label instead of adding external logic.

## Generic Arduino AVR Board

```toml
schema_version = 2
preset = "arduino-generic/json"

[controller]
hardware_include = "Arduino.h"
debug_serial = "Serial"
bridge_serial = "Serial"

[features]
fast_io = false

[devices.bench]
name = "bench"

[devices.bench.actuators.relay]
pin = "6"

[devices.bench.buttons.button]
pin = "2"
short = "relay"
```

Start generic boards with numeric Arduino pins and `fast_io = false`. Once the
profile builds and passes static analysis, enable faster codecs or direct-port
I/O deliberately.

## Release Checklist

Before flashing a real controller profile:

```bash
python3 tools/generate_lsh_static_config.py lsh_devices.toml --doctor
python3 tools/generate_lsh_static_config.py lsh_devices.toml --format-config
python3 tools/generate_lsh_static_config.py lsh_devices.toml --write-vscode-schema
python3 tools/generate_lsh_static_config.py lsh_devices.toml
python3 tools/generate_lsh_static_config.py lsh_devices.toml --check --check-format
```

Commit `lsh_devices.toml`, `lsh_devices.lock.toml` when IDs are auto-assigned,
and the local schema if you want editor autocomplete to work immediately after
checkout.
