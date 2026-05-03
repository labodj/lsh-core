# lsh-core Documentation Map

This page is the navigation hub for `lsh-core`.

Use it when you know the kind of answer you need, but not which document or example to
open first. The README stays focused on the first-use path; this page keeps the detailed
references together.

## Start Here

- **I am new to `lsh-core`**: read the [README](./README.md).
- **I want the TOML schema**: read
  [docs/static-toml-config.md](./docs/static-toml-config.md).
- **I want copyable configuration patterns**: read
  [docs/cookbook.md](./docs/cookbook.md).
- **I want compile-time tuning knobs**: read
  [docs/feature-flags.md](./docs/feature-flags.md).
- **I want hardware context**: read the
  [hardware integration section](./README.md#hardware-integration), then the stack-level
  hardware overview listed below.
- **I want API details**: use the
  [Doxygen API reference](https://labodj.github.io/lsh-core/).
- **I want the whole LSH stack first**: start from
  [`labo-smart-home`](https://github.com/labodj/labo-smart-home).

## Examples

| Example                                                          | Use it for                                                                 |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------- |
| [examples/multi-device-project](./examples/multi-device-project) | Main Controllino-style PlatformIO project with multiple generated profiles |
| [examples/avr-board-matrix](./examples/avr-board-matrix)         | Generic Arduino AVR compatibility matrix for Mega, Uno and Nano            |
| [examples/cookbook](./examples/cookbook)                         | Complete TOML profile used by the cookbook recipes                         |
| [examples/all-options-toml](./examples/all-options-toml)         | Generator-only catalog of every public schema v2 option                    |

## First Controller Path

For a first working controller, keep the path narrow:

1. Build [examples/multi-device-project](./examples/multi-device-project).
2. Reuse that project structure in your own PlatformIO project.
3. Edit `lsh_devices.toml`; do not edit generated headers by hand.
4. Keep the preset, serial settings and codec close to the example.
5. Add network-click behavior only after local buttons, relays and state publishing are
   healthy.

The detailed stack-level bring-up order lives in the
[`labo-smart-home` getting started guide](https://github.com/labodj/labo-smart-home/blob/main/GETTING_STARTED.md).

## Related Stack Docs

- [`labo-smart-home` documentation map](https://github.com/labodj/labo-smart-home/blob/main/DOCS.md)
- [Reference stack](https://github.com/labodj/labo-smart-home/blob/main/REFERENCE_STACK.md)
- [Hardware overview](https://github.com/labodj/labo-smart-home/blob/main/HARDWARE_OVERVIEW.md)
- [Troubleshooting](https://github.com/labodj/labo-smart-home/blob/main/TROUBLESHOOTING.md)
- [Protocol roles and profiles](https://github.com/labodj/lsh-protocol/blob/main/docs/profiles-and-roles.md)
- [Canonical wire contract](https://github.com/labodj/lsh-protocol/blob/main/shared/lsh_protocol.md)

## Maintainer Notes

- PlatformIO package release steps are in the
  [README](./README.md#platformio-registry-releases).
- The vendored protocol subtree is refreshed with `tools/update_lsh_protocol.py`.
- Python tooling configuration lives in [pyproject.toml](./pyproject.toml).
