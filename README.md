# LSH-Core: The Arduino Firmware Engine for Labo Smart Home

[![API Documentation](https://img.shields.io/badge/API%20Reference-Doxygen-blue.svg)](https://labodj.github.io/lsh-core/)

Welcome to `lsh-core`, the core firmware engine for the **Labo Smart Home (LSH)** ecosystem. This framework was originally designed for personal use with industrial-grade Controllino PLCs to ensure maximum reliability.

This document serves as the official guide for using the `lsh-core` library in your own PlatformIO projects.

## What is the Labo Smart Home (LSH) Ecosystem?

LSH is a complete, distributed home automation system composed of three distinct, open-source projects:

- **`lsh-core` (This Project):** The heart of the physical layer. This modern C++17 framework runs on an Arduino-compatible controller (like a Controllino). Its job is to read inputs (like push-buttons), control outputs (like relays and lights), and execute local logic with maximum speed and efficiency.

- **`lsh-bridge`:** A lightweight firmware designed for an ESP32. It acts as a semi-transparent bridge, physically connecting to `lsh-core` via serial and relaying messages to and from your network via MQTT. This isolates the core logic from Wi-Fi and network concerns.

- **[node-red-contrib-lsh-logic](https://github.com/labodj/node-red-contrib-lsh-logic):** A collection of nodes for Node-RED. This is the brain of your smart home, running on a server or Raspberry Pi. It listens to events from all your `lsh-core` devices and orchestrates complex, network-wide automation logic.

### System Architecture

The three components work together to create a robust and responsive system.

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
- `LSH_MAX_ACTUATORS`, `LSH_MAX_CLICKABLES`, and `LSH_MAX_INDICATORS` define **maximum accepted capacity**, not the real cardinality of the configured device.
- The real runtime counts are determined by how many times `addActuator()`, `addClickable()`, and `addIndicator()` are actually called.
- `lsh-core` sends a `BOOT` payload at startup. That payload invalidates any cached bridge-side model and forces a fresh `details + state` re-sync.
- A topology change is only supported through reflashing + reboot. Hot runtime topology changes are out of scope by design.
- The LSH protocol assumes a trusted environment: there is no built-in authentication or hardening against hostile peers on the serial link or MQTT path.
- Serial transport is codec-specific: JSON uses newline-delimited frames, while MsgPack uses a delimiter-and-escape framed transport.

### API Documentation

While this README provides a comprehensive guide for getting started and common use cases, a full, in-depth API reference is also available. This documentation is automatically generated using Doxygen from the source code comments and provides detailed information on all public classes, methods, and namespaces.

It is the perfect resource for developers who want to understand the inner workings of the library or explore advanced functionalities beyond the examples provided here.

**[Browse the full API Documentation here](https://labodj.github.io/lsh-core/)**

## Hardware & Electrical Setup

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

For a system-level hardware view of the real installation pattern, see the public landing repo:
[Labo Smart Home hardware overview](https://github.com/labodj/labo-smart-home/blob/main/HARDWARE_OVERVIEW.md)

## Getting Started: Creating Your Project

### 1. Project Setup

1. Create a new, blank PlatformIO project.
2. In your `platformio.ini`, add the LSH-Core library as a dependency:

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

3. Create the following directory structure inside your project:

   ```text
   LSH-User-Project/
   ├── platformio.ini
   ├── include/
   │   ├── lsh_user_config.hpp    # The "router" for your configurations
   │   └── lsh_configs/
   │       └── ... (your device header files go here)
   └── src/
       ├── main.cpp
       └── configs/
           └── ... (your device logic files go here)
   ```

### Core Configuration Concepts

All device-specific logic is defined in the `Configurator::configure()` function within your `src/configs/your_device.cpp` file. The LSH library provides a set of helper functions within the `Configurator` class to make this process clean and readable.

- `addActuator(Actuator* actuator)`: Registers an actuator with the system.
- `addClickable(Clickable* clickable)`: Registers a clickable with the system.
- `addIndicator(Indicator* indicator)`: Registers an indicator with the system.
- `getIndex(const Actuator& actuator)`: A crucial helper that returns the unique internal index of a registered actuator. You **must** use this function when connecting an actuator to a button or indicator, as shown below:

Important capacity rule:

- `LSH_MAX_*` macros size the fixed-capacity containers used by the firmware.
- The real number of registered devices can be lower than the declared maximum.
- For best RAM/code efficiency you will often set the maximum equal to the real count, but this is an optimization choice, not a functional requirement.

Optional bounded-ID lookup optimization:

- By default, `lsh-core` keeps actuator and clickable ID lookups in `etl::map`.
- If your IDs are numeric and stay inside a small, dense range, you can enable fixed O(1) lookup tables by also defining:
  - `LSH_MAX_ACTUATOR_ID`
  - `LSH_MAX_CLICKABLE_ID`
- If your IDs are strictly dense (`1..LSH_MAX_ACTUATORS` and `1..LSH_MAX_CLICKABLES` with no gaps), you can use the shorter opt-in flags instead:
  - `LSH_ASSUME_DENSE_ACTUATOR_IDS`
  - `LSH_ASSUME_DENSE_CLICKABLE_IDS`
- When these macros are present, `lsh-core` stores `index + 1` in a fixed ETL array sized to the declared maximum ID (`0` still means "missing").
- `LSH_MAX_*_ID` takes precedence over `LSH_ASSUME_DENSE_*_IDS`, so explicit max-ID declarations always win when both are present.
- This stays fully static and heap-free, but it is only RAM-efficient when the maximum ID is reasonably close to the real IDs you use. Sparse ID ranges should keep the default `etl::map`.

Optional compact actuator-link pools:

- By default, `lsh-core` reserves the worst-case link capacity for local logic:
  - every clickable short-click list can grow up to `LSH_MAX_ACTUATORS`
  - every clickable long-click list can grow up to `LSH_MAX_ACTUATORS`
  - every clickable super-long-click list can grow up to `LSH_MAX_ACTUATORS`
  - every indicator list can grow up to `LSH_MAX_ACTUATORS`
- If your real configuration uses far fewer links, you can reduce RAM by defining the actual total number of link entries used by the whole device:
  - `LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS`
  - `LSH_MAX_LONG_CLICK_ACTUATOR_LINKS`
  - `LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS`
  - `LSH_MAX_INDICATOR_ACTUATOR_LINKS`
- These values count real link entries, not devices. For example, one button with three long-click actuators contributes `3` to `LSH_MAX_LONG_CLICK_ACTUATOR_LINKS`.
- Count exactly what the configuration code appends:
  - every `addActuatorShort(...)` contributes `1` to `LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS`
  - every `addActuatorLong(...)` contributes `1` to `LSH_MAX_LONG_CLICK_ACTUATOR_LINKS`
  - every `addActuatorSuperLong(...)` contributes `1` to `LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS`
  - every `Indicator::addActuator(...)` contributes `1` to `LSH_MAX_INDICATOR_ACTUATOR_LINKS`
- Duplicates count too. If one clickable intentionally adds the same actuator twice, the pool must reserve two entries because the runtime stores exactly what the configuration asked for.
- Network-only clicks do not contribute local link entries by themselves. If a `setClickableLong(..., true, ...)` or `setClickableSuperLong(..., true, ...)` has no matching local `addActuatorLong(...)` or `addActuatorSuperLong(...)`, the local pool count for that click type stays unchanged.
- The storage stays static and heap-free. If you undersize one of these totals, setup aborts with a clear wrong-config reset to protect the compact pools from invalid writes.
- On AVR, omitting these macros now emits compile-time warnings because the compatibility fallback reserves the full worst-case `devices × actuators` budget in `.bss`.
- For final AVR builds you will usually want to define the real totals explicitly. The fallback is practical during early bring-up, but it is rarely the RAM-optimal end state.
- The bundled examples show both sides:
  - `J1` uses compact pools and disables network clicks completely
  - `J2` uses compact pools but keeps network clicks enabled

Optional network-click exclusion:

- If a device never uses network clicks, define `LSH_DISABLE_NETWORK_CLICKS`.
- This removes the network-click runtime state from the firmware instead of keeping dead arrays and timeout logic around.
- A device is a good candidate for this macro when no call to `setClickableLong(...)` or `setClickableSuperLong(...)` passes `networkClickable = true`.
- If a clickable is still marked as network-clickable while the feature is disabled, the runtime treats the network path as unavailable:
  - local fallback still runs when configured
  - otherwise the network-only action is skipped

Important registration-order rule:

- Register every `Clickable` with `addClickable(...)` before calling any `addActuatorShort(...)`, `addActuatorLong(...)`, or `addActuatorSuperLong(...)` on it.
- Register every `Actuator` with `addActuator(...)` before using `getIndex(actuator)` anywhere else.
- Register every `Indicator` with `addIndicator(...)` before calling `Indicator::addActuator(...)`.
- The compact pools store links using the dense runtime index assigned at registration time. Wrong registration order is treated as a setup error.
- Duplicate local links are rejected during setup. Duplicated links are almost always a configuration bug and can produce confusing behaviour such as one short click toggling the same relay twice.
- Every indicator must control at least one actuator. An empty indicator configuration is treated as a setup error.

Optional receive-path fairness guard:

- `CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP` bounds how many complete bridge payloads the controller dispatches in a single `loop()` iteration.
- `CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP` bounds how many raw UART bytes the controller may drain in the same iteration, including malformed or incomplete traffic.
- The defaults let one normal bridge burst make progress without allowing serial noise to monopolize the hot loop.
- Increase them only after measuring the real hardware tradeoff between bridge throughput and local button latency.

```cpp
// GOOD: Connects the button to the actuator using its safe index.
btn0.addActuatorShort(getIndex(rel0));

// BAD: This will not compile. You cannot pass the object directly.
// btn0.addActuatorShort(&rel0);
```

### 2. How to Add a New Device (e.g., "LivingRoom")

**Step 1: Create the Device Header (`.hpp`)**
Create `include/lsh_configs/living_room_config.hpp`. This file defines the hardware and build constants for this specific device.

```cpp
#ifndef LIVING_ROOM_CONFIG_HPP
#define LIVING_ROOM_CONFIG_HPP

// 1. Define the hardware library to include for this device.
#define LSH_HARDWARE_INCLUDE <Controllino.h>

// 2. Define the build constants required by the LSH library.
#define LSH_DEVICE_NAME "LivingRoom"
#define LSH_MAX_CLICKABLES 8
#define LSH_MAX_ACTUATORS 6
#define LSH_MAX_INDICATORS 2
#define LSH_MAX_CLICKABLE_ID 8
#define LSH_MAX_ACTUATOR_ID 6
#define LSH_MAX_SHORT_CLICK_ACTUATOR_LINKS 8
#define LSH_MAX_LONG_CLICK_ACTUATOR_LINKS 4
#define LSH_MAX_SUPER_LONG_CLICK_ACTUATOR_LINKS 2
#define LSH_MAX_INDICATOR_ACTUATOR_LINKS 3
#define LSH_COM_SERIAL &Serial1
#define LSH_DEBUG_SERIAL &Serial

#endif
```

If that device used dense IDs (`1..8` for clickables and `1..6` for actuators), the same optimization could be enabled with the shorter form:

```cpp
#define LSH_ASSUME_DENSE_CLICKABLE_IDS 1
#define LSH_ASSUME_DENSE_ACTUATOR_IDS 1
```

If that device had no network-click logic at all, it could also opt out completely:

```cpp
#define LSH_DISABLE_NETWORK_CLICKS 1
```

Those link totals are not guessed. They are meant to be derived from the real configuration source file that belongs to the same profile.

**Step 2: Create the Device Logic File (`.cpp`)**
Create `src/configs/living_room_config.cpp`. This is where you define your objects (relays, buttons) and their behavior.

```cpp
#include <lsh.hpp>  // Gives access to LSH_ACTUATOR, etc.

// Define all your device objects in an anonymous namespace to prevent name clashes.
namespace {
    LSH_ACTUATOR(mainLight, CONTROLLINO_R0, 1);
    LSH_BUTTON(wallSwitch, CONTROLLINO_A0, 1);
}

// Implement the configuration logic for this device.
void Configurator::configure() {
    addActuator(&mainLight);
    addClickable(&wallSwitch);
    wallSwitch.addActuatorShort(getIndex(mainLight));
}
```

**Step 3: Add the Device to the Build System**
First, tell the "router" header about your new device.
In `include/lsh_user_config.hpp`:

```cpp
#if defined(LSH_BUILD_LIVING_ROOM)
#include "lsh_configs/living_room_config.hpp"
#endif
```

Next, create the build environments in `platformio.ini`.

```ini
[device_LivingRoom]
device_feature_flags =
    -D CONFIG_MSG_PACK

[env:LivingRoom_release]
extends = common_release
build_src_filter = ${common_base.build_src_filter} +<configs/living_room_config.cpp>
build_flags =
    ${common_release.build_flags}
    ${common_base.default_feature_flags}
    ${device_LivingRoom.device_feature_flags}
    -D LSH_BUILD_LIVING_ROOM
```

## Configuring Device Behavior

All your logic is written inside the `Configurator::configure()` function in your device's `.cpp` file.

### Actuators (Relays)

Declare an actuator using the `LSH_ACTUATOR` macro. IDs must be unique and greater than 0.

```cpp
LSH_ACTUATOR(variable_name, PIN, UNIQUE_NUMERIC_ID);
```

#### Auto-Off Timer

Set a relay to automatically turn off after a predefined time (in milliseconds).

```cpp
rel0.setAutoOffTimer(600000);  // 10-minute timer
```

#### Protection

Protect a relay from being turned off by a global "all off" command (like a super-long click).

```cpp
rel0.setProtected(true);
```

### Clickables (Buttons)

Declare buttons using the `LSH_BUTTON` macro. IDs must be unique and greater than 0.

```cpp
LSH_BUTTON(variable_name, PIN, UNIQUE_NUMERIC_ID);
```

#### Short Click

A brief press of the button. It toggles the state of all associated relays. This is the default behavior.

```cpp
// This makes the button toggle rel0 on short click.
btn0.addActuatorShort(getIndex(rel0));
```

You can disable this behavior:

```cpp
btn0.setClickableShort(false);
```

#### Long Click

A press held longer than a short click.

```cpp
// Chain methods to add multiple relays to the long click action.
btn0.addActuatorLong(getIndex(rel1))
    .addActuatorLong(getIndex(rel2));

// Configure the long click behavior
btn0.setClickableLong(true, LongClickType::NORMAL);   // Default: turns ON if most are OFF, else turns OFF.
btn0.setClickableLong(true, LongClickType::ON_ONLY);  // Always turns relays ON.
btn0.setClickableLong(true, LongClickType::OFF_ONLY); // Always turns relays OFF.
```

#### Super-Long Click

A press held even longer.

```cpp
// By default, turns off ALL unprotected relays on the device.
btn0.setClickableSuperLong(true);

// Or, make it turn off only a specific list of relays.
btn0.addActuatorSuperLong(getIndex(rel0))
    .addActuatorSuperLong(getIndex(rel1));
btn0.setClickableSuperLong(true, SuperLongClickType::SELECTIVE);
```

#### Network Clicks

Long and super-long clicks can be forwarded over the network. You must specify a fallback behavior in case of network failure.

```cpp
// If the network fails, execute the long click action locally.
btn0.setClickableLong(true, LongClickType::ON_ONLY, true, NoNetworkClickType::LOCAL_FALLBACK);

// If the network fails, do nothing.
btn1.setClickableSuperLong(true, SuperLongClickType::NORMAL, true, NoNetworkClickType::DO_NOTHING);
```

### Indicators (LEDs)

Declare an indicator light using the `LSH_INDICATOR` macro.

```cpp
LSH_INDICATOR(variable_name, PIN);
```

Link one or more actuators to the indicator. Its behavior depends on the configured mode.

```cpp
// Link the indicator to two relays
statusLED.addActuator(getIndex(rel0));
statusLED.addActuator(getIndex(rel1));

// Configure the operating mode
statusLED.setMode(constants::IndicatorMode::ANY);      // Default: LED is ON if ANY linked relay is ON.
statusLED.setMode(constants::IndicatorMode::ALL);      // LED is ON only if ALL linked relays are ON.
statusLED.setMode(constants::IndicatorMode::MAJORITY); // LED is ON if more than half of the linked relays are ON.
```

### Network Clicks and Fallback Logic

A key feature of LSH is its ability to operate reliably both online and offline. Long clicks and super-long clicks can be configured to send a request over the network to `lsh-bridge` and `lsh-logic` for complex, multi-device automations. However, you must define what should happen if the network is unavailable. This is called **fallback logic**.

To enable a network click, set the third parameter of `setClickableLong()` or `setClickableSuperLong()` to `true`. The fourth parameter specifies the fallback behavior.

#### Fallback Types

You can choose between two different fallback types:

1. **`NoNetworkClickType::LOCAL_FALLBACK` (Default)**
   If a network problem occurs, the click is treated as a standard, local-only action. The actuators defined with `addActuatorLong()` or `addActuatorSuperLong()` for that button will be triggered on the device itself. This ensures the button always does _something_.

   ```cpp
   // This long click is a network action.
   // If the network is down, it will fall back to its local long-click logic.
   btn0.setClickableLong(true, LongClickType::ON_ONLY, true, NoNetworkClickType::LOCAL_FALLBACK);
   ```

2. **`NoNetworkClickType::DO_NOTHING`**
   If a network problem occurs, the click is simply ignored. This is useful for actions that only make sense in a network context (e.g., "All Lights Off" across the entire house).

   ```cpp
   // This super-long click is a network-only action.
   // If the network is down, pressing the button will have no effect.
   btn1.setClickableSuperLong(true, SuperLongClickType::NORMAL, true, NoNetworkClickType::DO_NOTHING);
   ```

#### The Network Communication Flow

Understanding the handshake between devices helps clarify when a fallback is triggered.

1. **Initial Request:** The user long-presses a network-enabled button on a Controllino running `lsh-core`.
2. `lsh-core` sends the click event (e.g., "Button ID 5, Long Click, Request") to the connected `lsh-bridge` (ESP32) and starts a short timeout timer.
3. **Gateway to MQTT:** `lsh-bridge` publishes the event to the MQTT broker (e.g., on topic `MyDevice/OUT`).
4. **Central Logic:** `lsh-logic` (Node-RED) receives the message, validates it against its configuration, and checks the status of any other devices involved.
5. **Acknowledgement (ACK):** If the request is valid, `lsh-logic` immediately sends an ACK back via MQTT (e.g., on topic `MyDevice/IN`).
6. **Confirmation:** `lsh-bridge` receives the ACK and forwards it to `lsh-core` via serial.
7. **Execution:** Upon receiving the ACK, `lsh-core` stops its timeout, confirms the action (e.g., with a quick LED blink), and sends a final confirmation message to `lsh-bridge` ("Button ID 5, Long Click, Confirmed").
8. **Final Action:** `lsh-logic` receives the final confirmation and executes the network-wide automation (e.g., turning on lights on three different devices).

The same bootstrapping contract is used outside of clicks:

- `lsh-core` sends `BOOT` during startup after configuration has been finalized.
- If the controller reboots while `lsh-bridge` is online, the bridge reboots immediately and rebuilds its cached model through the normal startup handshake.
- When `lsh-bridge` reaches `MQTT_READY`, it sends `BOOT` back to `lsh-core` to trigger a fresh `details + state` re-sync.
- This reboot-driven handshake is the only supported way to realign the bridge after reconnects. Runtime topology mutation without reboot is not part of the design.

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

This robust system ensures that the user gets immediate feedback and predictable behavior, whether the network is perfectly responsive or completely offline.

## Feature Flags

LSH-Core can be fine-tuned at compile-time using feature flags. These flags allow you to enable or disable specific functionalities to optimize for performance, memory usage, or specific hardware capabilities.

You can set these flags globally for all devices or on a per-device basis in your `platformio.ini` file.

### Communication Protocol

#### `CONFIG_MSG_PACK`

- **Description:** Switches the serial communication protocol between `lsh-core` and `lsh-bridge` from human-readable JSON to the more efficient, binary MessagePack format.
- **When to use:** Recommended for most production environments. MessagePack significantly reduces the size of the payloads, leading to faster and more reliable serial communication. This also reduces the RAM required for serialization buffers on both the Controllino and the ESP32.
- **Serial transport:** When this flag is enabled, the controller uses a framed MessagePack serial transport: `END + escaped(payload) + END`. JSON mode continues to use newline-delimited text frames.
- **Compile-time static payloads:** Static control payloads such as `BOOT` and `PING` are generated in both raw and serial-ready forms. `lsh-core` writes the serial-ready bytes directly to the UART, so static MessagePack control frames do not pay framing work at runtime.
- **Impact:** Smaller firmware size and lower RAM usage. Requires the `lsh-bridge` firmware to also be configured for MessagePack.

### I/O Performance

These flags replace standard `digitalRead()` and `digitalWrite()` calls with direct port manipulation for maximum speed. This is especially useful on AVR-based controllers like the ATmega2560, where it can dramatically reduce I/O latency.

#### `CONFIG_USE_FAST_CLICKABLES`

- **Description:** Optimizes the reading of input pins for buttons (`Clickable` objects).
- **When to use:** Always recommended unless you are using a non-standard board or core where direct port manipulation might not be supported. The performance gain ensures that even very rapid button presses are never missed.
- **Impact:** Faster input polling.

#### `CONFIG_USE_FAST_ACTUATORS`

- **Description:** Optimizes the writing to output pins for relays (`Actuator` objects).
- **When to use:** Always recommended for performance-critical applications.
- **Impact:** Faster relay switching.

#### `CONFIG_USE_FAST_INDICATORS`

- **Description:** Optimizes the writing to output pins for status LEDs (`Indicator` objects).
- **When to use:** Always recommended.
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

- **Default:** `RAW_INPUT_BUFFER_SIZE`
- **Description:** Bounds the total number of raw UART bytes that `lsh-core` may drain in one `loop()` iteration before returning to local input scanning and logic.
- **When to tune:** Raise it only if the bridge regularly delivers bursts that should be drained faster and hardware tests confirm that button latency stays acceptable.
- **Example:** `-D CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP=48U`

#### `CONFIG_COM_SERIAL_FLUSH_AFTER_SEND`

- **Default:** `1` (`enabled`)
- **Description:** Controls whether `lsh-core` calls `flush()` on the serial link after each payload sent to `lsh-bridge`.
- **Current status:** The system is currently validated with `flush()` enabled and in this configuration it works correctly and reliably.
- **Why this exists:** This flag is exposed only to evaluate whether the serial link remains resilient even without `flush()`, potentially reducing blocking time on send.
- **Recommendation:** Keep it enabled unless you are deliberately benchmarking or stress-testing the serial path without flush.
- **Examples:**
  - Keep the validated behavior: `-D CONFIG_COM_SERIAL_FLUSH_AFTER_SEND=1`
  - Experimental mode without flush: `-D CONFIG_COM_SERIAL_FLUSH_AFTER_SEND=0`

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
