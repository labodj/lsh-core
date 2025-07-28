# LSH-Core: The Arduino Firmware Engine for Labo Smart Home

Welcome to `lsh-core`, the core firmware engine for the **Labo Smart Home (LSH)** ecosystem. This framework was created by me and was originally designed for personal use with industrial-grade Controllino PLCs to ensure maximum reliability.

This document serves as the official guide for using the `lsh-core` library in your own PlatformIO projects.

## What is the Labo Smart Home (LSH) Ecosystem?

LSH is a complete, distributed home automation system composed of three distinct, open-source projects:

* **`lsh-core` (This Project):** The heart of the physical layer. This C++23 framework runs on an Arduino-compatible controller (like a Controllino). Its job is to read inputs (like push-buttons), control outputs (like relays and lights), and execute local logic with maximum speed and efficiency.

* **`lsh-bridge`:** A lightweight firmware designed for an ESP32. It acts as a transparent bridge, physically connecting to `lsh-core` via serial and relaying messages to and from your network via MQTT. This isolates the core logic from Wi-Fi and network concerns.

* **`node-red-contrib-lsh-logic`:** A collection of nodes for Node-RED. This is the brain of your smart home, running on a server or Raspberry Pi. It listens to events from all your `lsh-core` devices and orchestrates complex, network-wide automation logic.

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

## Hardware & Electrical Setup

`lsh-core` was designed around the **Controllino Maxi**, but can be adapted. The following setup is considered standard.

### Power Supply

The controller is typically powered by a **12V or 24V DC** power supply. This voltage is referred to as `VDD` throughout the electrical schematics.

### Push-Button Inputs

Each physical input pin is designed to be connected to one or more push-buttons. The standard wiring is:

> **INPUT PIN** ← Push-Button → **VDD**

When a button is pressed, it closes the circuit, connecting the input pin to `VDD` and signaling a "high" state to the controller.

### Output Wiring

* **High-Voltage Outputs (Relays):** The Controllino's relay outputs are used to switch high-voltage loads like 230V AC for lighting.
* **Low-Voltage Outputs (Digital Out):** These outputs provide a `VDD` signal and are typically used to power status LEDs on button panels.

### ESP32 (`lsh-bridge`) Connection

For network functionality, `lsh-core` communicates with an `lsh-bridge` device over a hardware serial port.

> **Crucial:** The Controllino operates at 5V logic, while the ESP32 operates at 3.3V. A **bi-directional logic level shifter** is **required** between them to prevent damage to the ESP32.

* **Controllino `TX` pin** → Logic Level Shifter (HV side) → (LV side) → **ESP32 `RX` pin**
* **Controllino `RX` pin** → Logic Level Shifter (HV side) → (LV side) → **ESP32 `TX` pin**

Typically, `Serial2` on the Controllino Maxi is used for this communication.

## Getting Started: Creating Your Project

### 1. Project Setup

1. Create a new, blank PlatformIO project.
2. In your `platformio.ini`, add the LSH-Core library as a dependency:

   ```ini
    [env:my_device]
    lib_deps = https://github.com/labodj/lsh-core.git
    ```

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

* `addActuator(Actuator* actuator)`: Registers an actuator with the system.
* `addClickable(Clickable* clickable)`: Registers a clickable with the system.
* `addIndicator(Indicator* indicator)`: Registers an indicator with the system.
* `getIndex(const Actuator& actuator)`: A crucial helper that returns the unique internal index of a registered actuator. You **must** use this function when connecting an actuator to a button or indicator, as shown below:

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
#define LSH_DEVICE_NAME      "LivingRoom"
#define LSH_MAX_CLICKABLES   8
#define LSH_MAX_ACTUATORS    6
#define LSH_MAX_INDICATORS   2
#define LSH_COM_SERIAL       &Serial1
#define LSH_DEBUG_SERIAL     &Serial

#endif
```

**Step 2: Create the Device Logic File (`.cpp`)**
Create `src/configs/living_room_config.cpp`. This is where you define your objects (relays, buttons) and their behavior.

```cpp
#include <lsh.hpp> // Gives access to LSH_ACTUATOR, etc.

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
src_filter = ${common_base.src_filter} +<configs/living_room_config.cpp>
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
    If a network problem occurs, the click is treated as a standard, local-only action. The actuators defined with `addActuatorLong()` or `addActuatorSuperLong()` for that button will be triggered on the device itself. This ensures the button always does *something*.

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

#### When is Fallback Logic Triggered?

The configured fallback logic is applied instantly if any step in this chain fails:

* The `lsh-bridge` (ESP32) is physically disconnected or unreachable.
* The `lsh-bridge` has no Wi-Fi connection or cannot reach the MQTT broker.
* The `lsh-logic` controller sends a negative acknowledgement (NACK) because the request is invalid or other devices are offline.
* **Most importantly: If the initial ACK from `lsh-logic` does not arrive back at the `lsh-core` device within the timeout period (typically ~1 second).**

This robust system ensures that the user gets immediate feedback and predictable behavior, whether the network is perfectly responsive or completely offline.

## Feature Flags

LSH-Core can be fine-tuned at compile-time using feature flags. These flags allow you to enable or disable specific functionalities to optimize for performance, memory usage, or specific hardware capabilities.

You can set these flags globally for all devices or on a per-device basis in your `platformio.ini` file.

### Communication Protocol

#### `CONFIG_MSG_PACK`

* **Description:** Switches the serial communication protocol between `lsh-core` and `lsh-bridge` from human-readable JSON to the more efficient, binary MessagePack format.
* **When to use:** Recommended for most production environments. MessagePack significantly reduces the size of the payloads, leading to faster and more reliable serial communication. This also reduces the RAM required for serialization buffers on both the Controllino and the ESP32.
* **Impact:** Smaller firmware size and lower RAM usage. Requires the `lsh-bridge` firmware to also be configured for MessagePack.

### I/O Performance

These flags replace standard `digitalRead()` and `digitalWrite()` calls with direct port manipulation for maximum speed. This is especially useful on AVR-based controllers like the ATmega2560, where it can dramatically reduce I/O latency.

#### `CONFIG_USE_FAST_CLICKABLES`

* **Description:** Optimizes the reading of input pins for buttons (`Clickable` objects).
* **When to use:** Always recommended unless you are using a non-standard board or core where direct port manipulation might not be supported. The performance gain ensures that even very rapid button presses are never missed.
* **Impact:** Faster input polling.

#### `CONFIG_USE_FAST_ACTUATORS`

* **Description:** Optimizes the writing to output pins for relays (`Actuator` objects).
* **When to use:** Always recommended for performance-critical applications.
* **Impact:** Faster relay switching.

#### `CONFIG_USE_FAST_INDICATORS`

* **Description:** Optimizes the writing to output pins for status LEDs (`Indicator` objects).
* **When to use:** Always recommended.
* **Impact:** Faster LED state changes.

### Timing Configuration

These flags allow you to override the default timing behavior of the framework. You typically don't need to define these unless you have specific hardware or user experience requirements.

#### `CONFIG_CLICKABLE_DEBOUNCE_TIME_MS`

* **Default:** `20U` (20 milliseconds)
* **Description:** Sets the debounce time for all buttons. This is the minimum time a button state must be stable before being recognized as a valid press or release, preventing electrical noise from causing multiple triggers.
* **Example:** `-D CONFIG_CLICKABLE_DEBOUNCE_TIME_MS=30U`

#### `CONFIG_CLICKABLE_LONG_CLICK_TIME_MS`

* **Default:** `400U` (400 milliseconds)
* **Description:** Sets the time a button must be held down to be registered as a "long click".
* **Example:** `-D CONFIG_CLICKABLE_LONG_CLICK_TIME_MS=500U`

#### `CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS`

* **Default:** `1000U` (1000 milliseconds)
* **Description:** Sets the time a button must be held down to be registered as a "super-long click".
* **Example:** `-D CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS=1500U`

#### `CONFIG_LCNB_TIMEOUT_MS`

* **Default:** `1000U` (1000 milliseconds)
* **Description:** Sets the timeout for network clicks. If `lsh-core` sends a network click request and does not receive an ACK within this period, it will trigger the configured fallback logic.
* **Example:** `-D CONFIG_LCNB_TIMEOUT_MS=1200U`

### Network and Communication Buffers

#### `CONFIG_PING_INTERVAL_MS`

* **Default:** `10000U` (10 seconds)
* **Description:** Sets the interval at which `lsh-core` sends a "ping" message to `lsh-bridge` to keep the connection alive and verify that the bridge is responsive.
* **Example:** `-D CONFIG_PING_INTERVAL_MS=15000U`

#### `CONFIG_CONNECTION_TIMEOUT_MS`

* **Default:** `PING_INTERVAL_MS + 200U`
* **Description:** The duration after the last received message from `lsh-bridge` before `lsh-core` considers the connection to be lost.
* **Example:** `-D CONFIG_CONNECTION_TIMEOUT_MS=15500U`

### Benchmarking (for developers)

These flags are intended for development and performance testing of the LSH-Core library itself.

#### `CONFIG_LSH_BENCH`

* **Description:** Enables a simple benchmarking routine in the main `loop()`. It measures the time taken to complete a fixed number of empty loop iterations.
* **When to use:** Only for library development or performance tuning to measure the overhead of the core loop. This should be disabled in production.

#### `CONFIG_BENCH_ITERATIONS`

* **Default:** `1000000U` (1 million)
* **Description:** Sets the number of iterations for the benchmark loop enabled by `CONFIG_LSH_BENCH`.
* **Example:** `-D CONFIG_BENCH_ITERATIONS=500000U`

## Building and Uploading

Use the standard PlatformIO commands from within your user project folder, specifying the target environment.

```bash
# Build the 'J1_release' environment
platformio run -e J1_release

# Build and upload the 'J1_debug' environment
platformio run -e J1_debug --target upload
```
