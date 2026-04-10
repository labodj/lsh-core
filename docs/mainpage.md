@mainpage LSH-Core Documentation

# LSH-Core

`lsh-core` is the deterministic firmware engine for the Labo Smart Home ecosystem.
It runs on the controller-side MCU, owns the physical topology, and executes the
local logic for buttons, actuators, indicators, failover behaviour and the serial
protocol spoken with `lsh-esp`.

## What This Documentation Covers

This Doxygen site combines three complementary sources:

- the public API extracted from `include/` and `src/`
- the integration and operational guide from `README.md`
- the generated wire-protocol reference from `vendor/lsh-protocol/shared/lsh_protocol.md`

Use the API reference when you need class- and method-level details, and use the
README / protocol pages when you need the system contract around the library.

## Runtime Invariants

The firmware is intentionally strict about a few invariants:

- The configured topology is static between two controller boots.
- `LSH_MAX_ACTUATORS`, `LSH_MAX_CLICKABLES`, and `LSH_MAX_INDICATORS` define fixed-capacity limits, not the real runtime cardinality.
- The real cardinality is the number of successful `addActuator()`, `addClickable()`, and `addIndicator()` registrations performed by `Configurator::configure()`.
- Device IDs exposed on the wire are positive non-zero `uint8_t` values and must stay unique within their domain.
- `BOOT` is the re-synchronization trigger used to invalidate cached models in the bridge and force a fresh `details + state` cycle.
- The protocol assumes a trusted environment. Authentication, encryption, and hostile-peer hardening are intentionally out of scope.

## Navigation

- Start with `README.md` for project structure, build flags, fallback logic and integration examples.
- Inspect @ref Configurator to understand how devices are declared and registered.
- Inspect @ref Clickable, @ref Actuator and @ref Indicator for the local I/O model.
- Inspect @ref LSH::protocol for the generated constants mirrored from the shared protocol spec.
- Inspect the generated protocol page included from `vendor/lsh-protocol/shared/lsh_protocol.md` for the canonical key map, command IDs and golden payload examples.

## Design Notes

The codebase is optimized for embedded determinism:

- fixed-capacity ETL containers instead of heap allocation
- compact on-wire payloads
- compile-time feature flags for timing, debugging and performance tuning
- explicit fallback behaviour when the network path is unavailable

That optimization target matters when reviewing or extending the code. Prefer
changes that preserve RAM predictability, keep hot paths branch-light, and do
not relax the boot/resync contract between `lsh-core` and `lsh-esp`.
