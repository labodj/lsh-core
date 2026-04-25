# lsh-core multi-device PlatformIO example

This example is the quickest way to see how a real `lsh-core` consumer is laid
out after the `v3.0.0` TOML configuration change.

The important files are:

- `lsh_devices.toml`: human-authored device topology and feature defines
- `platformio.ini`: PlatformIO environments and the static-config pre-build hook
- `include/lsh_user_config.hpp`: generated profile router
- `include/lsh_configs/*_config.hpp`: generated per-device compile-time options
- `include/lsh_configs/*_static_config.hpp`: generated static topology and
  behavior implementation
- `src/main.cpp`: normal Arduino entry point calling `lsh::core::setup()` and
  `lsh::core::loop()`

Do not edit the generated headers by hand. Edit `lsh_devices.toml` and rebuild.

## Build

From the `lsh-core` repository root:

```bash
platformio run -d examples/multi-device-project -e J1_release
platformio run -d examples/multi-device-project -e J1_debug
platformio run -d examples/multi-device-project -e J2_release
platformio run -d examples/multi-device-project -e J2_debug
```

## Profiles

- `J1_release`: leaner MsgPack profile with local behavior and no network-click actions
- `J2_release`: MsgPack profile with network-click actions enabled
- debug profiles add `LSH_DEBUG` and keep the debug serial path active

Both profiles inherit `CONFIG_MSG_PACK` and the three fast-I/O flags from
`[common.defines]`, matching the AVR priority used by this repository: runtime
speed first, SRAM second and flash third.

The selected controller is set with `custom_lsh_device` in each environment.
The pre-build hook reads that value, validates the TOML, generates the matching
headers, and adds the internal `LSH_BUILD_*` macro for the compiler.

## Adapting This Example

For a first custom controller:

1. Copy this directory into your own PlatformIO project.
2. Rename the device keys in `lsh_devices.toml`.
3. Update actuator, clickable and indicator names, IDs and pins.
4. Keep the topology, codec and serial choices close to the example until the first
   controller-to-bridge path works.
5. Add network-click behavior only after local buttons, relays and state
   publishing are healthy.
