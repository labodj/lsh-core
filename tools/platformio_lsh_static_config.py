"""PlatformIO pre-build hook for TOML-backed lsh-core static profiles."""

from __future__ import annotations

import inspect
import sys
from pathlib import Path
from typing import TYPE_CHECKING, Protocol, cast

if TYPE_CHECKING:
    from collections.abc import Sequence


CppDefine = str | tuple[str, str]


class PlatformIOEnv(Protocol):
    """Subset of the SCons construction environment used by this hook."""

    def GetProjectOption(self, name: str) -> object:  # noqa: N802
        """Return the active PlatformIO project option."""

    def subst(self, template: str) -> str:
        """Expand a SCons construction variable expression."""

    def Append(  # noqa: N802
        self,
        *,
        CPPDEFINES: Sequence[CppDefine],  # noqa: N803
        CPPPATH: Sequence[str],  # noqa: N803
        BUILD_FLAGS: Sequence[str],  # noqa: N803
    ) -> None:
        """Append generated build settings to the active environment."""


def _load_platformio_env() -> PlatformIOEnv:
    """Import and return PlatformIO's injected `env` object."""
    importer = globals().get("Import")
    if not callable(importer):
        message = "PlatformIO did not inject the SCons Import function."
        raise TypeError(message)
    importer("env")
    return cast("PlatformIOEnv", globals()["env"])


def _current_script_dir() -> Path:
    """Return this script directory even when SCons omits `__file__`."""
    frame = inspect.currentframe()
    if frame is None:
        message = "Cannot resolve PlatformIO extra script location."
        raise RuntimeError(message)
    return Path(frame.f_code.co_filename).resolve().parent


env = _load_platformio_env()
TOOLS_DIR = _current_script_dir()
REPO_ROOT = TOOLS_DIR.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools import generate_lsh_static_config as lsh_static_config  # noqa: E402


def _get_option(name: str, default: str | None = None) -> str | None:
    """Read one scalar custom option from the active PlatformIO environment."""
    value = env.GetProjectOption(name)
    if value in (None, ""):
        return default
    if not isinstance(value, str):
        message = f"PlatformIO option {name!r} must be a scalar string."
        raise TypeError(message)
    return value


project_dir = Path(env.subst("$PROJECT_DIR")).resolve()
config_option = _get_option("custom_lsh_config", "lsh_devices.toml")
if config_option is None:
    message = "custom_lsh_config resolved to an empty value."
    raise RuntimeError(message)
config_path = Path(config_option)
if not config_path.is_absolute():
    config_path = project_dir / config_path

project = lsh_static_config.parse_project(config_path.resolve())
requested_device = _get_option("custom_lsh_device")
device_key = (
    lsh_static_config.resolve_device_key(project, requested_device)
    if requested_device is not None
    else lsh_static_config.infer_device_from_env(project, env.subst("$PIOENV"))
)
device = project.devices[device_key]

lsh_static_config.generate(project, [device_key], check=False)

cpp_defines: list[CppDefine] = []
for define in lsh_static_config.merged_defines(project, device):
    if define.value is None:
        cpp_defines.append(define.name)
    else:
        cpp_defines.append((define.name, define.value))

env.Append(
    CPPDEFINES=cpp_defines,
    CPPPATH=[str(project.settings.output_dir)],
    BUILD_FLAGS=lsh_static_config.raw_build_flags(project, device),
)

sys.stdout.write(
    f"lsh-core: generated static config for {device_key} from {config_path.name}\n"
)
