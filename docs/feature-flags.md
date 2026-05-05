# Feature Flags and Compile-Time Tuning

`lsh-core` is designed for small controllers, so some choices are fixed at compile time.
For typical projects, start with semantic TOML fields and only fall back to raw
`CONFIG_*` defines when a profile needs a setting that the schema does not expose yet.

For schema v2, prefer this shape:

```toml
[features]
codec = "msgpack"
fast_io = true

[timing]
button_debounce = "20ms"
long_click = "400ms"

[serial]
bridge_baud = 500000
max_rx_bytes_per_loop = 64
```

Use the advanced override only when needed:

```toml
[advanced.defines]
CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP = 64
```

PlatformIO-wide defaults can still live in `platformio.ini` when they are intentionally
shared by every environment.

## Recommended Baseline

For AVR static profiles, the public examples assume this priority order: runtime speed
first, SRAM second, flash third.

That is why the Controllino reference profile enables MsgPack and direct-port I/O by
default. Keep JSON only when bridge compatibility, flash size or human-readable serial
frames matter more than throughput.

## Communication Protocol

### `CONFIG_MSG_PACK`

- **Description:** Switches the controller-to-bridge serial payloads from
  newline-delimited JSON to framed binary MessagePack.
- **When to use:** Recommended for AVR profiles where runtime speed and SRAM are more
  important than flash size and human-readable serial output.
- **Transport:** MsgPack uses `END + escaped(payload) + END`. JSON remains
  newline-delimited.
- **Tradeoff:** Smaller dynamic payloads and less text parsing, but more codec and
  framing code in flash. `lsh-bridge` must use the same serial codec.

## I/O Performance

These flags replace Arduino helper calls with direct port access where the selected AVR
board supports it. They are good defaults for Controllino and ATmega2560-style profiles
where the button scan path is performance-sensitive.

### `CONFIG_USE_FAST_CLICKABLES`

- **Description:** Uses the fast input path for button reads.
- **When to use:** Keep enabled on supported AVR boards. Disable first on unknown boards
  or unusual cores.
- **Impact:** Faster input polling.

### `CONFIG_USE_FAST_ACTUATORS`

- **Description:** Uses the fast output path for relay writes.
- **When to use:** Keep enabled for performance-sensitive profiles on supported boards.
- **Impact:** Faster relay switching.

### `CONFIG_USE_FAST_INDICATORS`

- **Description:** Uses the fast output path for indicator writes.
- **When to use:** Keep enabled on supported boards.
- **Impact:** Faster LED updates.

## Timing

| Define                                          | Default                  | Meaning                                                     |
| ----------------------------------------------- | ------------------------ | ----------------------------------------------------------- |
| `CONFIG_ACTUATOR_DEBOUNCE_TIME_MS`              | `100U`                   | Minimum interval between two switches of the same actuator. |
| `CONFIG_CLICKABLE_DEBOUNCE_TIME_MS`             | `20U`                    | Button debounce threshold.                                  |
| `CONFIG_CLICKABLE_SCAN_INTERVAL_MS`             | `1U`                     | Minimum elapsed time between input scan passes.             |
| `CONFIG_CLICKABLE_LONG_CLICK_TIME_MS`           | `400U`                   | Default long-click threshold.                               |
| `CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS`     | `1000U`                  | Default super-long-click threshold.                         |
| `CONFIG_LCNB_TIMEOUT_MS`                        | `1000U`                  | Compatibility default for network-click timeout settings.   |
| `CONFIG_NETWORK_CLICK_ACK_TIMEOUT_MS`           | `CONFIG_LCNB_TIMEOUT_MS` | ACK timeout before fallback is applied.                     |
| `CONFIG_NETWORK_CLICK_CONFIRM_RETRY_TIMEOUT_MS` | `CONFIG_LCNB_TIMEOUT_MS` | Retry budget for `NETWORK_CLICK_CONFIRM` after ACK is seen. |

`CONFIG_CLICKABLE_SCAN_INTERVAL_MS` is a scan policy setting, not a hard real-time
guarantee. If the controller is busy, elapsed time is still passed through the button
state machine so debounce and click timing stay coherent.

Tune scan intervals only after measuring the real tradeoff between button latency,
serial fairness and CPU headroom.

## Serial and Bridge Runtime

| Define                                            | Default                        | Meaning                                                                   |
| ------------------------------------------------- | ------------------------------ | ------------------------------------------------------------------------- |
| `CONFIG_PING_INTERVAL_MS`                         | `10000U`                       | Interval for controller-to-bridge pings.                                  |
| `CONFIG_CONNECTION_TIMEOUT_MS`                    | `PING_INTERVAL_MS + 200U`      | Time after the last bridge message before the link is considered lost.    |
| `CONFIG_BRIDGE_BOOT_RETRY_INTERVAL_MS`            | `250U`                         | BOOT retry interval while bridge startup is incomplete.                   |
| `CONFIG_BRIDGE_AWAIT_STATE_TIMEOUT_MS`            | `1500U`                        | Timeout while waiting for the bridge to request authoritative state.      |
| `CONFIG_DEBUG_SERIAL_BAUD`                        | `115200U`                      | Debug serial baud when `LSH_DEBUG` is enabled.                            |
| `CONFIG_COM_SERIAL_BAUD`                          | `250000U`                      | Controller-to-bridge UART baud.                                           |
| `CONFIG_COM_SERIAL_TIMEOUT_MS`                    | `5U`                           | Compatibility timeout used as the default incomplete-frame cleanup value. |
| `CONFIG_COM_SERIAL_MSGPACK_FRAME_IDLE_TIMEOUT_MS` | `CONFIG_COM_SERIAL_TIMEOUT_MS` | Timeout used to drop one incomplete framed MsgPack payload.               |
| `CONFIG_COM_SERIAL_MAX_RX_PAYLOADS_PER_LOOP`      | mode-derived                   | Maximum complete bridge payloads dispatched in one loop pass.             |
| `CONFIG_COM_SERIAL_MAX_RX_BYTES_PER_LOOP`         | mode-derived                   | Maximum raw UART bytes drained in one loop pass.                          |
| `CONFIG_COM_SERIAL_FLUSH_AFTER_SEND`              | debug: `1`, release: `0`       | Whether to flush the serial link after each payload.                      |
| `CONFIG_DELAY_AFTER_RECEIVE_MS`                   | `50U`                          | Quiet window after bridge-side state-changing payloads.                   |
| `CONFIG_NETWORK_CLICK_CHECK_INTERVAL_MS`          | `50U`                          | Pending network-click polling interval.                                   |
| `CONFIG_ACTUATORS_AUTO_OFF_CHECK_INTERVAL_MS`     | `1000U`                        | Auto-off scan interval.                                                   |

Keep the receive fairness guards on their mode-derived defaults for a first build. Raise
them only if the bridge regularly delivers bursts that should be drained faster and
hardware tests confirm that local button latency stays acceptable.

## Benchmarking

### `CONFIG_LSH_BENCH`

- **Description:** Enables the developer loop benchmark.
- **When to use:** Only for library development or performance tuning.
- **Production:** Keep disabled.

### `CONFIG_BENCH_ITERATIONS`

- **Default:** `1000000U`
- **Description:** Number of iterations used by the benchmark loop.

## ETL Profile Override

`lsh-core` ships with a default [`etl_profile.h`](../include/etl_profile.h) for the
common Arduino/PlatformIO case. It sets only the library policy settings that are part
of the current project assumptions while ETL continues to auto-detect compiler and
language support through `etl/profiles/auto.h`.

If a target needs a different ETL setup, use a small project-owned override:

1. Create a header such as `include/lsh_etl_profile_override.h`.
2. Pass `LSH_ETL_PROFILE_OVERRIDE_HEADER` and point it at that header.
3. In the header, redefine only the ETL settings the target really needs.

Example:

```cpp
// include/lsh_etl_profile_override.h
#pragma once

#undef ETL_CHECK_PUSH_POP
#define ETL_THROW_EXCEPTIONS
```

The bundled multi-device example demonstrates the same hook in
[`examples/multi-device-project/include/lsh_etl_profile_override.h`](../examples/multi-device-project/include/lsh_etl_profile_override.h)
and its matching PlatformIO flag.
