# lsh-core all-options TOML catalog

This directory contains a generator-only TOML catalog. It is not meant to model
one realistic controller; it exists to show every currently accepted public
configuration option in one place.

CI validates it by copying `lsh_devices.toml` to a temporary directory,
generating headers there, and running the generator's stale-output check. If you
run the generator in this directory, local output is written under `generated/`,
which is intentionally ignored by git.
