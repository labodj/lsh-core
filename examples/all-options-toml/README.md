# lsh-core all-options TOML catalog

This directory contains a generator-only TOML catalog. It is not meant to model
one realistic controller; it exists to show every currently accepted public
schema v2 option in one place, including the semantic fields and the expert
escape hatches. The catalog also includes groups, scenes, interlocked actuators
and one pulse actuator so the generated static wrappers exercise the advanced
local-action paths.

CI validates it by copying `lsh_devices.toml` to a temporary directory,
generating headers there, and running the generator's stale-output check. If you
run the generator in this directory, local output is written under `generated/`,
which is intentionally ignored by git.

`lsh_devices.lock.toml` is generated from the catalog and committed on purpose:
it documents the stable ID mapping that a real consumer should keep beside its
TOML profile.
