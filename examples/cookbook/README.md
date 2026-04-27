# lsh-core Cookbook Example

This directory contains one complete `lsh_devices.toml` profile used by the
official cookbook. It is intentionally generator-focused: the file is meant to
show real schema v2 patterns that can be copied into a consumer project.

Validate it from the repository root:

```bash
python3 tools/generate_lsh_static_config.py examples/cookbook/lsh_devices.toml --doctor
python3 tools/generate_lsh_static_config.py examples/cookbook/lsh_devices.toml --check-format
python3 tools/generate_lsh_static_config.py examples/cookbook/lsh_devices.toml --write-vscode-schema
```

The generated `.vscode/lsh_devices.schema.json` is project-specific and includes
the actuator, group and scene names from this example.
