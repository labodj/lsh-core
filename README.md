# lsh-core: Controller Firmware for Labo Smart Home

[![Build Status](https://github.com/labodj/lsh-core/actions/workflows/ci.yml/badge.svg)](https://github.com/labodj/lsh-core/actions/workflows/ci.yml)
[![Latest Release](https://img.shields.io/github/v/release/labodj/lsh-core?display_name=tag&sort=semver)](https://github.com/labodj/lsh-core/releases/latest)
[![API Documentation](https://img.shields.io/badge/API%20Reference-Doxygen-blue.svg)](https://labodj.github.io/lsh-core/)

`lsh-core` is the controller-side firmware library for the **Labo Smart Home
(LSH)** ecosystem. It runs on Arduino-compatible controllers, reads wired
inputs, drives relays and indicators, and talks to an ESP32 bridge over serial.

The default public path is a Controllino-style AVR controller paired with
`lsh-bridge`, MQTT and Node-RED. The library is still useful to study or reuse
on its own, but the documented adoption path assumes the full stack.

Since `v3.0.0`, user configuration is TOML-first. You describe devices,
buttons, relays, pins and click behavior in `lsh_devices.toml`; the generator
emits the optimized C++ profile before compilation. A typical device profile
does not require C++ setup code for topology.

The hosted GitHub Pages API reference tracks the latest tagged release so the
public class-level documentation stays aligned with released artifacts. This
README on `main` may describe newer work that has not been tagged yet.

If you are new to the public LSH stack, read the landing repository and its
reference profile first:

- [Labo Smart Home landing page](https://github.com/labodj/labo-smart-home)
- [LSH reference stack](https://github.com/labodj/labo-smart-home/blob/main/REFERENCE_STACK.md)
- [LSH glossary](https://github.com/labodj/labo-smart-home/blob/main/GLOSSARY.md)

## What You Need

For the documented controller path:

- PlatformIO
- Python 3.11 or newer for the TOML generator
- an Arduino-compatible AVR target; Controllino Maxi is the best documented one
- an ESP32 running `lsh-bridge` if you want MQTT/Homie integration
- an MQTT broker and Node-RED if you want the full public reference behavior

The library is optimized for static device topology. If your project needs
devices to appear and disappear at runtime, this is not the right abstraction
without additional work.

## Start Here

Use this README in different ways depending on what you need:

- If you are new to LSH, start with the landing page, the reference stack and the glossary before reading this firmware guide.
- If you want the shortest answers to common adoption questions, skim the landing [`FAQ.md`](https://github.com/labodj/labo-smart-home/blob/main/FAQ.md).
- If you want the shortest end-to-end bring-up path, read the landing [`GETTING_STARTED.md`](https://github.com/labodj/labo-smart-home/blob/main/GETTING_STARTED.md) before customizing this firmware.
- If your first lab is partially alive but inconsistent, use the landing [`TROUBLESHOOTING.md`](https://github.com/labodj/labo-smart-home/blob/main/TROUBLESHOOTING.md).
- If you want to wire a controller correctly, jump to [Hardware & Electrical Setup](#hardware--electrical-setup).
- If you want to build your first controller project, jump to [Getting Started: Creating Your Project](#getting-started-creating-your-project).
- If you want click semantics, fallbacks and network behavior, jump to [Configuring Device Behavior](#configuring-device-behavior).
- If you want compile-time tuning knobs, jump to [Feature Flags](#feature-flags).
- If you want class- and method-level details for the latest released API, use the [Doxygen API reference](https://labodj.github.io/lsh-core/).

## Bundled Example

The fastest concrete starting point in this repository is:

- [examples/multi-device-project](./examples/multi-device-project)

It already shows a reusable multi-device PlatformIO layout with separate device
profiles, a TOML source file and generated headers.

Useful example profiles:

- `J1_release`: leaner profile, MsgPack enabled, no network-click subsystem
- `J2_release`: richer profile that keeps the network-click path enabled

Build it directly from this repository:

```bash
platformio run -d examples/multi-device-project -e J1_release
platformio run -d examples/multi-device-project -e J2_release
```

For the stack-level bring-up order around this example, use the landing
[`GETTING_STARTED.md`](https://github.com/labodj/labo-smart-home/blob/main/GETTING_STARTED.md).

## What is the Labo Smart Home (LSH) Ecosystem?

LSH is a complete, distributed home automation system composed of four public,
open-source repositories:

- **`lsh-core` (This Project):** The heart of the physical layer. This modern C++17 framework runs on an Arduino-compatible controller (like a Controllino). Its job is to read inputs (like push-buttons), control outputs (like relays and lights), and execute local logic with maximum speed and efficiency.

- **`lsh-bridge`:** A lightweight firmware designed for an ESP32. It acts as a semi-transparent bridge, physically connecting to `lsh-core` via serial and relaying messages to and from your network via MQTT. This isolates the core logic from Wi-Fi and network concerns.

- **[node-red-contrib-lsh-logic](https://github.com/labodj/node-red-contrib-lsh-logic):** A collection of nodes for Node-RED. This is the brain of your smart home, running on a server or Raspberry Pi. It listens to events from all your `lsh-core` devices and orchestrates complex, network-wide automation logic.

- **[lsh-protocol](https://github.com/labodj/lsh-protocol):** The shared protocol source of truth. It keeps command IDs, compact keys, compatibility metadata and generated artifacts aligned across the controller, bridge and Node-RED layers.

### Runtime Path

The active runtime path involves three peers. `lsh-protocol` sits beside them as
the shared contract that keeps the payload model aligned.

```text
+-----------------+                      +-----------------+                      +-----------------+
|   lsh-core      | --(1) Click Event--> |   lsh-bridge    | --(2) MQTT Publish-> |   MQTT Broker   |
|(Physical Layer) |      [Serial]        | (Gateway/Bridge)|                      |  (Message Hub)  |
|                 | <----(7) Command---- |                 | <----(6) Command---- |                 |
+-----------------+      [Serial]        +-----------------+                      +--------+--------+
                                                                                           |
                                                                                 (3) Event |
                                                                                           v
                                                                                 +--------+--------+
                                                                                 | lsh-logic (NR)  |
                                                                                 |  (Logic Layer)  |
                                                                                 | --(5) Command --+
                                                                                 +-----------------+
```

### Operational Invariants

The serial contract between `lsh-core` and `lsh-bridge` is intentionally strict:

- The device topology is built during `Configurator::configure()` and is considered static until the next controller reboot.
- Generated `LSH_STATIC_CONFIG_ACTUATORS`, `LSH_STATIC_CONFIG_CLICKABLES`, and `LSH_STATIC_CONFIG_INDICATORS` define the exact static capacity of the selected device.
- The runtime counts are produced by the generated registration pass and must match those static capacities before the controller enters the main loop.
- `lsh-core` sends a `BOOT` payload at startup. That payload invalidates any cached bridge-side model and forces a fresh `details + state` re-sync.
- A topology change is only supported through reflashing + reboot. Hot runtime topology changes are out of scope by design.
- The LSH protocol assumes a trusted environment: there is no built-in authentication or hardening against hostile peers on the serial link or MQTT path.
- Serial transport is codec-specific: JSON uses newline-delimited frames, while MsgPack uses a delimiter-and-escape framed transport.

### API Documentation

While this README provides a comprehensive guide for getting started and common use cases, a full, in-depth API reference is also available. This documentation is automatically generated using Doxygen from the source code comments and provides detailed information on all public classes, methods, and namespaces.

Use it when you need class-level details, method signatures or implementation
notes beyond the examples in this README.

The hosted site tracks the latest tagged release. If you are reading `main`
between releases, the repository sources and this README may already include
changes that are not reflected on the published API pages yet.

**[Browse the full API Documentation here](https://labodj.github.io/lsh-core/)**

## Hardware & Electrical Setup

This section keeps the `lsh-core`-specific electrical assumptions. For the full
public panel pattern and the cross-repo controller/bridge split, see:

- [Labo Smart Home hardware overview](https://github.com/labodj/labo-smart-home/blob/main/HARDWARE_OVERVIEW.md)
- [LSH reference stack](https://github.com/labodj/labo-smart-home/blob/main/REFERENCE_STACK.md)

`lsh-core` was designed around the **Controllino Maxi**, but can be adapted. The following setup is considered standard.

### Power Supply

The controller is typically powered by a **12V or 24V DC** power supply. This voltage is referred to as `VDD` throughout the electrical schematics.

### Push-Button Inputs

Each physical input pin is designed to be connected to one or more push-buttons. The standard wiring is:

> **INPUT PIN** ← Push-Button → **VDD**

When a button is pressed, it closes the circuit, connecting the input pin to `VDD` and signaling a "high" state to the controller.

### Output Wiring

- **Relay Outputs:** The Controllino relay outputs can be used to switch loads at **12 V / 24 V / 115 V / 230 V**, within the limits documented by the official Controllino datasheet and the rest of the installation.
- **Low-Voltage Outputs (Digital Out):** These outputs provide a `VDD` signal and are typically used to power status LEDs and illuminated push-buttons on button panels.

Typical field-model assumptions in the real installation:

- wall push-buttons stay on the low-voltage side and are fed from the same controller supply (`VDD`)
- indicator lights also stay at the controller supply voltage
- the controller owns the direct relationship between field inputs, relays and indicator outputs

### ESP32 (`lsh-bridge`) Connection

For network functionality, `lsh-core` communicates with an `lsh-bridge` device over a hardware serial port.

> **Crucial:** The Controllino operates at 5V logic, while the ESP32 operates at 3.3V. A **bi-directional logic level shifter** is **required** between them to prevent damage to the ESP32.

- **Controllino `TX` pin** → Logic Level Shifter (HV side) → (LV side) → **ESP32 `RX` pin**
- **Controllino `RX` pin** → Logic Level Shifter (HV side) → (LV side) → **ESP32 `TX` pin**

Typically, `Serial2` on the Controllino Maxi is used for this communication.

### Local-First Runtime Boundary

`lsh-core` is meant to own the deterministic part of the installation.

- short-click logic, relay ownership and indicator behavior live on the controller
- network-assisted logic extends the device behavior, but should not be the only thing making the panel usable
- when Wi-Fi, MQTT or the central logic node are unavailable, local behavior should still remain coherent

This is why the bridge and orchestration layers are treated as additive rather than authoritative over the physical panel.

## Getting Started: Creating Your Project

### 1. Project Setup

1. Create a new, blank PlatformIO project.
2. In your `platformio.ini`, add `lsh-core` as a dependency:

   ```ini
    [env:my_device]
    platform = atmelavr
    framework = arduino
    board = controllino_maxi
    build_unflags = -std=gnu++11 -std=c++11
    build_flags =
        -I include
        -std=gnu++17
    lib_deps = https://github.com/labodj/lsh-core.git
   ```

   If you are building the bundled example inside this repository, keep the local
   `lsh-core=symlink://../..` dependency used by
   `examples/multi-device-project/platformio.ini`.

3. Add the generator hook and select a device profile:

   ```ini
   extra_scripts = pre:path/to/lsh-core/tools/platformio_lsh_static_config.py
   custom_lsh_config = lsh_devices.toml

   [env:my_device]
   custom_lsh_device = my_device
   ```

   The `extra_scripts` path must point to an accessible `lsh-core` checkout.
   The bundled example uses a local symlink dependency; consumer projects can
   use an adjacent checkout, a submodule or another fixed local path.

4. Create the following directory structure inside your project:

   ```text
   LSH-User-Project/
   ├── platformio.ini
   ├── lsh_devices.toml          # Human-authored device topology
   ├── include/
   │   ├── lsh_user_config.hpp    # Generated router header
   │   └── lsh_configs/
   │       └── ... generated device headers
   └── src/
       └── main.cpp
   ```

5. Write `lsh_devices.toml`, then build. PlatformIO runs the generator before
   compilation and injects the correct `LSH_BUILD_*` selector for the selected
   `custom_lsh_device`.

For a complete working layout, copy the shape of
[examples/multi-device-project](./examples/multi-device-project) instead of
starting from a blank file.

### Core Configuration Concepts

Device-specific topology is described in `lsh_devices.toml`. The pre-build
generator validates that file and emits the static C++ profile consumed by
`Configurator::configure()`.

The generated profile calls the same low-level registration API that older
hand-written profiles used, but most users never touch those calls directly:

- `addActuator(Actuator* actuator)`: Registers an actuator with the system.
- `addClickable(Clickable* clickable)`: Registers a clickable with the system.
- `addIndicator(Indicator* indicator)`: Registers an indicator with the system.
- `getIndex(const Actuator& actuator)`: Resolves the dense runtime actuator index used by generated links.

Keep the TOML as the source of truth and regenerate the headers. The generated
profile owns registration order, dense indexes, resource counts and lookup
accessors.

Generated capacity rule:

- The generator emits exact `LSH_STATIC_CONFIG_*` resource macros for the selected profile.
- `src/internal/user_config_bridge.hpp` imports those macros and exposes internal `CONFIG_*` `constexpr` values for allocation code.
- Fixed-capacity containers are therefore sized from the real topology, not from hand-maintained worst-case numbers.
- Zero-count resources are still represented with one physical ETL slot where ETL requires a strictly positive array capacity; the logical count remains zero and the extra slot is never used.

Generated ID lookup:

- Public actuator and clickable IDs may be sparse as long as they stay in `1..255`.
- The generator emits branch/range accessors for `id -> dense index` and `dense index -> id`; no user-authored lookup tables are needed.
- The highest accepted ID is generated as `LSH_STATIC_CONFIG_MAX_ACTUATOR_ID` and `LSH_STATIC_CONFIG_MAX_CLICKABLE_ID`.

Generated actuator-link pools:

- Short, long, super-long and indicator link totals are counted from the TOML.
- Duplicate local targets inside one action are rejected by the generator.
- Network-only clicks do not consume local link entries unless they also list local fallback targets.
- Runtime storage stays static and heap-free; generated compile-time checks reject counts outside the supported AVR-friendly field widths.

Generated runtime pools:

- Per-click timing overrides are counted from `long.time` and `super_long.time`.
- Auto-off pool size is counted from actuators with non-zero `auto_off`.
- Active network-click capacity is counted from configured network actions; one held button with both long and super-long network clicks needs two active transactions.
- `LSH_COMPACT_ACTUATOR_SWITCH_TIMES` remains a user-facing optimization define. It removes the per-`Actuator` 32-bit switch timestamp and keeps timestamps only for auto-off actuators. It requires `CONFIG_ACTUATOR_DEBOUNCE_TIME_MS=0` so debounce semantics remain exact.

Optional network-click exclusion:

- If a device never uses `network = true`, the generator emits a static profile with the network-click runtime compiled out.
- `LSH_NETWORK_CLICKS = false` in TOML rejects network-click actions for that profile at generation time.
- `LSH_NETWORK_CLICKS = true` can force the runtime path on for experiments, but normal profiles should let the generator derive it.

Generated validation rule:

- The generator registers actuators, clickables and indicators in a deterministic order.
- It rejects missing references, duplicated targets, empty indicators, disabled actions with active options and unsupported path/identifier expressions before compilation.
- `Configurator::finalizeSetup()` still validates compact manager invariants before runtime starts.

Controllino setup helpers:

- On Controllino Maxi / Maxi Automation / Mega profiles, `Configurator::configure()` can call `disableRtc()` and `disableEth()`.
- `disableRtc()` forces the onboard RTC chip select inactive when the AVR profile does not use the RTC.
- `disableEth()` forces the Ethernet controller chip select inactive when Ethernet is not owned by the AVR firmware.
- TOML fields `disable_rtc = true` and `disable_eth = true` emit these calls for the selected static profile.

Compile-time constants layout:

- User profile macros are imported by `src/internal/user_config_bridge.hpp` and exposed as `CONFIG_*` values used by low-level allocation code.
- The same resource-limit values are also mirrored under `constants::config` for documentation-oriented code and future references.
- Timing constants live in `src/util/constants/timing.hpp`.
- Serial/bridge constants live in `src/communication/constants/config.hpp`.

Optional receive-path fairness guard:

- `CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP` bounds how many complete bridge payloads the controller dispatches in a single `loop()` iteration.
- `CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP` bounds how many raw UART bytes the controller may drain in the same iteration, including malformed or incomplete traffic.
- The defaults let one normal bridge burst make progress without allowing serial noise to monopolize the hot loop.
- Increase them only after measuring the real hardware tradeoff between bridge throughput and local button latency.

### 2. How to Add a New Device (e.g., "LivingRoom")

**Step 1: Let the Generator Own the Headers**

Do not write `include/lsh_user_config.hpp`, `include/lsh_configs/*_config.hpp`
or resource-count macros by hand for new devices. The TOML generator creates
those files and derives exact counts from the real topology, including sparse
IDs, link totals, network-click pool size, auto-off timers and timing overrides.

**Step 2: Describe the Device in TOML**

Create `lsh_devices.toml` in your consumer project. Users configure names,
public IDs, pins and click behavior; the generator emits the C++ objects,
resource counts and lookup accessors.

```toml
[generator]
output_dir = "include"
config_dir = "lsh_configs"
user_config_header = "lsh_user_config.hpp"

[common]
hardware_include = "Controllino.h"
debug_serial = "Serial"
com_serial = "Serial2"

[devices.living_room]
name = "LivingRoom"

[[devices.living_room.actuators]]
name = "mainLight"
id = 1
pin = "CONTROLLINO_R0"

[[devices.living_room.clickables]]
name = "wallSwitch"
id = 1
pin = "CONTROLLINO_A0"
short = ["mainLight"]
```

**Step 3: Add the Generator to the Build System**

Create the build environments in `platformio.ini`. The pre-build hook validates
the TOML, writes `include/lsh_user_config.hpp`, generates the selected static
profile and adds the correct `LSH_BUILD_*` macro.

```ini
[common_base]
extra_scripts = pre:path/to/lsh-core/tools/platformio_lsh_static_config.py
custom_lsh_config = lsh_devices.toml
build_src_filter = +<*> -<configs/>

[env:LivingRoom_release]
extends = common_release
custom_lsh_device = living_room
build_src_filter = ${common_base.build_src_filter}
build_flags =
    ${common_release.build_flags}
    ${common_base.default_feature_flags}
```

## Configuring Device Behavior

The current public configuration surface is TOML. New profiles should follow
[docs/static-toml-config.md](docs/static-toml-config.md); the build generates
the static C++ profile from that file and keeps dense indexes, resource counts
and lookup accessors out of user-authored code.

The bundled [examples/all-options-toml](examples/all-options-toml) catalog shows
every accepted TOML option and is validated by CI. Use it as a syntax reference;
use [examples/multi-device-project](examples/multi-device-project) as the
buildable starting point.

The sections below use the public TOML format. Generated C++ remains an
implementation detail.

### Actuators (Relays)

Declare an actuator in TOML. IDs must be unique in the device and stay in the
wire range `1..255`.

```toml
[[devices.living_room.actuators]]
name = "main_light"
id = 1
pin = "CONTROLLINO_R0"
default_state = false
protected = false
auto_off = "10m"
```

The `pin` value must be a compile-time Arduino expression such as a board macro
(`CONTROLLINO_R0`, `CONTROLLINO_A0`, ...) or a numeric literal. On supported AVR
boards, the generator lets `lsh-core` resolve the final port/mask binding at
compile time while keeping the hot write path on direct register access.

`auto_off` accepts durations such as `"900ms"`, `"30s"`, `"10m"` or `"1h"`.
`protected = true` excludes that relay from global all-off super-long actions.

### Clickables (Buttons)

Declare inputs in TOML and reference actuators by name. The generator resolves
those names into dense indexes and exact link pools before compilation.

```toml
[[devices.living_room.clickables]]
name = "wall_switch"
id = 1
pin = "CONTROLLINO_A0"
short = ["main_light"]
long = { targets = ["main_light"], type = "on_only", time = "900ms" }
super_long = { type = "selective", targets = ["main_light"] }
```

Short clicks toggle their local targets. Long clicks support `normal`,
`on_only`/`on-only` and `off_only`/`off-only`. Super-long clicks support
`normal` global all-off behavior or `selective` target lists. Both long and
super-long actions can set `network = true` and choose a fallback policy.

### Indicators (LEDs)

Declare an indicator and the actuators it watches:

```toml
[[devices.living_room.indicators]]
name = "main_light_led"
pin = "CONTROLLINO_D0"
actuators = ["main_light"]
mode = "any"
```

`mode` can be `any`, `all` or `majority`.

### Network Clicks and Fallback Logic

A key feature of LSH is its ability to operate reliably both online and offline. Long clicks and super-long clicks can be configured to send a request over the network to `lsh-bridge` and `lsh-logic` for complex, multi-device automations. However, you must define what should happen if the network is unavailable. This is called **fallback logic**.

To enable a network click, set `network = true` on the TOML `long` or
`super_long` action. The `fallback` field specifies what happens if the network
path is unavailable.

If the same button has both long and super-long network clicks enabled, `lsh-core` preserves the natural sequence for a held press: the long network click is requested first, then the super-long network click is requested while the button remains pressed. The generator accounts for that single button as two active network-click slots.

#### Fallback Types

You can choose between two different fallback types:

1. **`local` / `local_fallback` (Default)**
   If a network problem occurs, the click is treated as a standard, local-only action. The actuators listed in the same action's `targets` field will be triggered on the device itself. This ensures the button always does _something_.

   ```toml
   long = { network = true, fallback = "local", targets = ["main_light"], type = "on_only" }
   ```

2. **`do_nothing` / `do-nothing`**
   If a network problem occurs, the click is simply ignored. This is useful for actions that only make sense in a network context (e.g., "All Lights Off" across the entire house).

   ```toml
   super_long = { network = true, fallback = "do_nothing" }
   ```

#### The Network Communication Flow

Understanding the handshake between devices helps clarify when a fallback is triggered.

1. **Initial Request:** The user long-presses a network-enabled button on a Controllino running `lsh-core`.
2. `lsh-core` sends the click event (e.g., "Button ID 5, Long Click, Request") to the connected `lsh-bridge` (ESP32) and starts a short timeout timer.
3. **Gateway to MQTT:** `lsh-bridge` publishes the request to the controller-backed MQTT runtime topic (for example `LSH/<device>/events`).
4. **Central Logic:** `lsh-logic` (Node-RED) receives the message, validates it against its configuration, and checks the status of any other devices involved.
5. **Acknowledgement (ACK):** If the request is valid, `lsh-logic` immediately sends `NETWORK_CLICK_ACK` back on the device command topic (for example `LSH/<device>/IN`).
6. **Confirmation:** `lsh-bridge` receives the ACK and forwards it to `lsh-core` via serial.
7. **Execution:** Upon receiving the ACK, `lsh-core` stops its timeout, confirms the action (e.g., with a quick LED blink), and sends `NETWORK_CLICK_CONFIRM` back through `lsh-bridge`.
8. **Final Action:** `lsh-logic` receives the final confirmation and executes the network-wide automation (e.g., turning on lights on three different devices).

The same bootstrapping contract is used outside of clicks:

- `lsh-core` sends `BOOT` during startup after configuration has been finalized.
- When the bridge receives controller `BOOT`, it stops trusting controller-derived runtime state and requests fresh `DEVICE_DETAILS`.
- After validated details are accepted, the bridge requests fresh `ACTUATORS_STATE` before it treats the controller path as synchronized again.
- If the bridge has no validated cached topology yet, or if the topology changed, it persists the new details and performs one controlled reboot so MQTT topics and Homie nodes are rebuilt from a coherent snapshot.
- MQTT reconnects do not redefine the serial protocol. The bridge re-subscribes and re-synchronizes its MQTT-side runtime around the cached or freshly confirmed controller model.
- A bridge-local service-topic `BOOT` may be used by orchestration peers to request a replay when snapshots are missing. That is a profile behavior of the public stack, not a mandatory end-to-end forwarding rule for `BOOT`.

For the public reference profile behind this flow, see:

- [LSH reference stack](https://github.com/labodj/labo-smart-home/blob/main/REFERENCE_STACK.md)
- [vendor/lsh-protocol/docs/profiles-and-roles.md](vendor/lsh-protocol/docs/profiles-and-roles.md)

For the canonical command IDs, compact key map and golden JSON examples generated from the shared spec, see [vendor/lsh-protocol/shared/lsh_protocol.md](vendor/lsh-protocol/shared/lsh_protocol.md).

The protocol maintenance workflow itself is documented once in the vendored subtree README at `vendor/lsh-protocol/README.md`. This README only keeps the `lsh-core`-specific invariants and runtime behavior.

To verify that the generated protocol files in this repository are aligned with the vendored source of truth:

```bash
python3 tools/update_lsh_protocol.py --check
```

#### When is Fallback Logic Triggered?

The configured fallback logic is applied instantly if any step in this chain fails:

- The `lsh-bridge` (ESP32) is physically disconnected or unreachable.
- The `lsh-bridge` has no Wi-Fi connection or cannot reach the MQTT broker.
- The `lsh-logic` controller sends a negative acknowledgement (NACK) because the request is invalid or other devices are offline.
- **Most importantly: If the initial ACK from `lsh-logic` does not arrive back at the `lsh-core` device within the timeout period (typically ~1 second).**

This keeps user feedback predictable whether the network path is healthy,
slow or unavailable.

## Feature Flags

LSH-Core can be fine-tuned at compile-time using feature flags. These flags allow you to enable or disable specific functionalities to optimize for performance, memory usage, or specific hardware capabilities.

For TOML-backed profiles, put per-device flags in `[devices.<name>.defines]`.
PlatformIO-only global defaults can still live in `platformio.ini` when they are
intentionally shared by every environment.

### Communication Protocol

#### `CONFIG_MSG_PACK`

- **Description:** Switches the serial communication protocol between `lsh-core` and `lsh-bridge` from human-readable JSON to the more efficient, binary MessagePack format.
- **When to use:** Recommended for most production environments. MessagePack significantly reduces the size of the payloads, leading to faster and more reliable serial communication. This also reduces the RAM required for serialization buffers on both the Controllino and the ESP32.
- **Serial transport:** When this flag is enabled, the controller uses a framed MessagePack serial transport: `END + escaped(payload) + END`. JSON mode continues to use newline-delimited text frames.
- **Compile-time static payloads:** Static control payloads such as `BOOT` and `PING` are generated in both raw and serial-ready forms. `lsh-core` writes the serial-ready bytes directly to the UART, so static MessagePack control frames do not pay framing work at runtime.
- **Impact:** Smaller firmware size and lower RAM usage. Requires the `lsh-bridge` firmware to also be configured for MessagePack.

### I/O Performance

These flags replace standard `digitalRead()` and `digitalWrite()` calls with direct port manipulation for maximum speed. This is especially useful on AVR-based controllers like the ATmega2560, where it can dramatically reduce I/O latency.

When the device is declared through the public `LSH_*` macros and the selected
pin is a compile-time constant, the AVR fast-I/O path also resolves the final
register binding at compile time on supported Mega/Controllino-class boards.
The hot path still uses the same cached direct register access as before; only
the setup-time lookup changes. Unsupported boards or pins fall back to the
traditional Arduino table lookup path automatically.

#### `CONFIG_USE_FAST_CLICKABLES`

- **Description:** Optimizes the reading of input pins for buttons (`Clickable` objects).
- **When to use:** Always recommended unless you are using a non-standard board or core where direct port manipulation might not be supported. The performance gain ensures that even very rapid button presses are never missed.
- **Compile-time path:** With a generated static profile and a compile-time pin constant, supported AVR boards avoid the setup-time Arduino lookup tables entirely and still keep the polling path as one direct register read.
- **Impact:** Faster input polling.

#### `CONFIG_USE_FAST_ACTUATORS`

- **Description:** Optimizes the writing to output pins for relays (`Actuator` objects).
- **When to use:** Always recommended for performance-critical applications.
- **Compile-time path:** With a generated static profile and a compile-time pin constant, supported AVR boards resolve the port binding at compile time while leaving the steady-state write path as a direct register update.
- **Impact:** Faster relay switching.

#### `CONFIG_USE_FAST_INDICATORS`

- **Description:** Optimizes the writing to output pins for status LEDs (`Indicator` objects).
- **When to use:** Always recommended.
- **Compile-time path:** With a generated static profile and a compile-time pin constant, supported AVR boards resolve the indicator binding at compile time and keep runtime LED updates on the direct port path.
- **Impact:** Faster LED state changes.

### Timing Configuration

These flags allow you to override the default timing behavior of the framework. You typically don't need to define these unless you have specific hardware or user experience requirements.

#### `CONFIG_ACTUATOR_DEBOUNCE_TIME_MS`

- **Default:** `100U` (100 milliseconds)
- **Description:** Sets the minimum delay between two consecutive switches of the same actuator. This protects relays and other outputs from overly rapid toggling caused by noisy or repeated commands.
- **Example:** `-D CONFIG_ACTUATOR_DEBOUNCE_TIME_MS=150U`

#### `CONFIG_CLICKABLE_DEBOUNCE_TIME_MS`

- **Default:** `20U` (20 milliseconds)
- **Description:** Sets the debounce time for all buttons. This is the minimum time a button state must be stable before being recognized as a valid press or release, preventing electrical noise from causing multiple triggers.
- **Example:** `-D CONFIG_CLICKABLE_DEBOUNCE_TIME_MS=30U`

#### `CONFIG_CLICKABLE_SCAN_INTERVAL_MS`

- **Default:** `1U` (1 millisecond)
- **Description:** Sets the minimum elapsed time between two input scan passes. With the default value, the historical policy remains approximately `~1000 Hz` when the main loop is otherwise free to run.
- **Behavior note:** This is a scan policy knob, not a hard real-time guarantee. If the controller is busy, `lsh-core` passes the whole accumulated elapsed time to the clickable state machine so debounce and long-click timing stay coherent.
- **Bridge note:** Bridge heartbeat pacing and handshake retries use their own elapsed-time gate and are not paced by this input scan interval.
- **When to tune:** Increase it only after measuring the real hardware tradeoff between button latency, serial fairness and CPU headroom.
- **Example:** `-D CONFIG_CLICKABLE_SCAN_INTERVAL_MS=2U`

#### `CONFIG_CLICKABLE_LONG_CLICK_TIME_MS`

- **Default:** `400U` (400 milliseconds)
- **Description:** Sets the time a button must be held down to be registered as a "long click".
- **Example:** `-D CONFIG_CLICKABLE_LONG_CLICK_TIME_MS=500U`

#### `CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS`

- **Default:** `1000U` (1000 milliseconds)
- **Description:** Sets the time a button must be held down to be registered as a "super-long click".
- **Example:** `-D CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS=1500U`

#### `CONFIG_LCNB_TIMEOUT_MS`

- **Default:** `1000U` (1000 milliseconds)
- **Description:** Sets the timeout for network clicks. If `lsh-core` sends a network click request and does not receive an ACK within this period, it will trigger the configured fallback logic.
- **Example:** `-D CONFIG_LCNB_TIMEOUT_MS=1200U`

### Network and Communication Buffers

#### `CONFIG_PING_INTERVAL_MS`

- **Default:** `10000U` (10 seconds)
- **Description:** Sets the interval at which `lsh-core` sends a "ping" message to `lsh-bridge` to keep the connection alive and verify that the bridge is responsive.
- **Example:** `-D CONFIG_PING_INTERVAL_MS=15000U`

#### `CONFIG_CONNECTION_TIMEOUT_MS`

- **Default:** `PING_INTERVAL_MS + 200U`
- **Description:** The duration after the last received message from `lsh-bridge` before `lsh-core` considers the connection to be lost.
- **Example:** `-D CONFIG_CONNECTION_TIMEOUT_MS=15500U`

#### `CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS`

- **Default:** `250U` (250 milliseconds)
- **Description:** Sets how often `lsh-core` retries the bridge bootstrap handshake after sending `BOOT`, while the bridge has not yet completed its startup sequence.
- **Example:** `-D CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS=500U`

#### `CONFIG_BRIDGE_AWAIT_STATE_TIMEOUT_MS`

- **Default:** `1500U` (1500 milliseconds)
- **Description:** Sets how long `lsh-core` waits for the bridge to request the authoritative state after the device details have already been sent. If this timeout expires, the bootstrap handshake restarts from `BOOT`.
- **Example:** `-D CONFIG_BRIDGE_AWAIT_STATE_TIMEOUT_MS=2000U`

#### `CONFIG_DEBUG_SERIAL_BAUD`

- **Default:** `115200U`
- **Description:** Sets the baud rate used by the debug serial port when `LSH_DEBUG` is enabled.
- **Example:** `-D CONFIG_DEBUG_SERIAL_BAUD=500000U`

#### `CONFIG_COM_SERIAL_BAUD`

- **Default:** `250000U`
- **Description:** Sets the baud rate of the controller-to-bridge serial link used to talk to `lsh-bridge`.
- **Example:** `-D CONFIG_COM_SERIAL_BAUD=500000U`

#### `CONFIG_COM_SERIAL_TIMEOUT_MS`

- **Default:** `5U` (5 milliseconds)
- **Description:** Defines the compatibility fallback used as the default value for `CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS`.
- **Behavior note:** The current receive path does not use timeout-based framing. Changing this flag only changes the default housekeeping timeout for incomplete MsgPack frames unless you also override `CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS`.
- **Example:** `-D CONFIG_COM_SERIAL_TIMEOUT_MS=10U`

#### `CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS`

- **Default:** `CONFIG_COM_SERIAL_TIMEOUT_MS`
- **Description:** Sets the housekeeping timeout used to drop one incomplete framed MsgPack payload after the UART goes silent for too long. This timeout only cleans up truncated frames; it does not define frame boundaries.
- **Example:** `-D CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS=8U`

#### `CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP`

- **Default:** `RAW_INPUT_BUFFER_SIZE` in JSON mode, `MSGPACK_SERIAL_MAX_FRAME_SIZE` in MsgPack mode
- **Description:** Bounds the total number of raw UART bytes that `lsh-core` may drain in one `loop()` iteration before returning to local input scanning and logic.
- **When to tune:** Raise it only if the bridge regularly delivers bursts that should be drained faster and hardware tests confirm that button latency stays acceptable.
- **Example:** `-D CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP=48U`

#### `CONFIG_COM_SERIAL_FLUSH_AFTER_SEND`

- **Default:** `1` in `LSH_DEBUG` builds, `0` in release builds
- **Description:** Controls whether `lsh-core` calls `flush()` on the serial link after each payload sent to `lsh-bridge`.
- **Current status:** Debug builds keep the conservative validated behavior, while release builds avoid the blocking flush unless explicitly requested.
- **Why this exists:** This flag lets a profile force the conservative behavior or explicitly benchmark the non-flushing serial path.
- **Recommendation:** Keep the release default unless hardware tests show that the bridge link needs flushes on that specific installation.
- **Examples:**
  - Keep the validated behavior: `-D CONFIG_COM_SERIAL_FLUSH_AFTER_SEND=1`
  - Force the release-style non-blocking send path: `-D CONFIG_COM_SERIAL_FLUSH_AFTER_SEND=0`

#### `CONFIG_DELAY_AFTER_RECEIVE_MS`

- **Default:** `50U` (50 milliseconds)
- **Description:** Sets the short quiet window used after receiving a bridge-side state-changing payload before `lsh-core` mirrors the new authoritative state back out. This reduces duplicate publish bursts when multiple single-actuator updates arrive close together.
- **Example:** `-D CONFIG_DELAY_AFTER_RECEIVE_MS=75U`

#### `CONFIG_NETWORK_CLICK_CHECK_INTERVAL_MS`

- **Default:** `50U` (50 milliseconds)
- **Description:** Sets how often pending network-click requests are revisited to detect ACK timeouts and trigger fallback logic when needed.
- **Example:** `-D CONFIG_NETWORK_CLICK_CHECK_INTERVAL_MS=25U`

#### `CONFIG_ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS`

- **Default:** `1000U` (1 second)
- **Description:** Sets how often `lsh-core` scans actuators with auto-off timers to decide whether they must be turned off.
- **Example:** `-D CONFIG_ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS=250U`

### Benchmarking (for developers)

These flags are intended for development and performance testing of the LSH-Core library itself.

#### `CONFIG_LSH_BENCH`

- **Description:** Enables a simple benchmarking routine in the main `loop()`. It measures the time taken to complete a fixed number of empty loop iterations.
- **When to use:** Only for library development or performance tuning to measure the overhead of the core loop. This should be disabled in production.

#### `CONFIG_BENCH_ITERATIONS`

- **Default:** `1000000U` (1 million)
- **Description:** Sets the number of iterations for the benchmark loop enabled by `CONFIG_LSH_BENCH`.
- **Example:** `-D CONFIG_BENCH_ITERATIONS=500000U`

### ETL profile override

`lsh-core` ships with a default [etl_profile.h](./include/etl_profile.h) so the
common Arduino/PlatformIO case works out of the box.

That default profile intentionally sets only the library policy knobs that are
part of the current project assumptions, while ETL still auto-detects the
active compiler and language support through `etl/profiles/auto.h`.

If you need a different ETL setup for another target or toolchain, the
recommended override path is:

1. Create your own small header in the consumer project, for example `include/lsh_etl_profile_override.h`
2. Pass the `LSH_ETL_PROFILE_OVERRIDE_HEADER` build flag and point it at your override header.
3. In that header, `#undef` and redefine only what you need

Example:

```cpp
// include/lsh_etl_profile_override.h
#pragma once

#undef ETL_CHECK_PUSH_POP
#define ETL_THROW_EXCEPTIONS
```

If your build system prefers full ownership, you may also provide your own
project-level `etl_profile.h` earlier in the include path and bypass the one
shipped by `lsh-core`.

The bundled example project already demonstrates this hook through
[examples/multi-device-project/include/lsh_etl_profile_override.h](./examples/multi-device-project/include/lsh_etl_profile_override.h)
and the matching `LSH_ETL_PROFILE_OVERRIDE_HEADER` flag in
[examples/multi-device-project/platformio.ini](./examples/multi-device-project/platformio.ini).

## Building and Uploading

Use the standard PlatformIO commands from within your user project folder, specifying the target environment.

```bash
# Build the 'J1_release' environment
platformio run -e J1_release

# Build and upload the 'J1_debug' environment
platformio run -e J1_debug --target upload
```
