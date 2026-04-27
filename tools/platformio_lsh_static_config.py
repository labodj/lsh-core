"""PlatformIO pre-build hook for TOML-backed lsh-core static profiles."""

from __future__ import annotations

import inspect
import os
import re
import sys
from pathlib import Path
from typing import TYPE_CHECKING, Protocol, cast

if TYPE_CHECKING:
    from collections.abc import Sequence


CppDefine = str | tuple[str, str]
MIN_INCLUDE_OPERAND_LENGTH = 3
GENERATED_HEADER_DEPENDENCY_DEPTH = 2
INCLUDE_DIRECTIVE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')


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


def _include_operand_path(include_operand: str) -> Path | None:
    """Extract the path-like part from a C++ include operand.

    lsh-core intentionally lets a profile choose the board support header at
    compile time. PlatformIO's dependency finder can miss that generated include
    when the user routes config headers through custom paths, so this hook adds a
    direct include directory for matching installed libdeps when possible.
    """
    if len(include_operand) < MIN_INCLUDE_OPERAND_LENGTH:
        return None
    if include_operand[0] == "<" and include_operand[-1] == ">":
        return Path(include_operand[1:-1])
    if include_operand[0] == '"' and include_operand[-1] == '"':
        return Path(include_operand[1:-1])
    return None


def _matching_header_locations(header: Path, root: Path) -> list[Path]:
    """Return installed headers whose suffix matches the requested include path."""
    if not header.parts or not root.is_dir():
        return []

    locations: list[Path] = []
    suffix_length = len(header.parts)
    for candidate in sorted(root.rglob(header.name)):
        if not candidate.is_file() or candidate.parts[-suffix_length:] != header.parts:
            continue
        locations.append(candidate)
    return locations


def _include_root_for_header(header: Path, location: Path) -> str:
    """Return the include directory that makes `header` visible at `location`."""
    return str(location.parents[len(header.parts) - 1])


def _header_includes(location: Path) -> list[Path]:
    """Read simple include directives from one generated hardware header."""
    includes: list[Path] = []
    try:
        lines = location.read_text(encoding="utf-8", errors="ignore").splitlines()
    except OSError:
        return includes
    for line in lines:
        match = INCLUDE_DIRECTIVE_RE.match(line)
        if match is not None:
            includes.append(Path(match.group(1)))
    return includes


def _platformio_header_search_roots() -> list[Path]:
    """Return PlatformIO package roots worth scanning for generated includes."""
    roots = [Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")]

    package_dirs = [
        Path(candidate)
        for candidate in (
            env.subst("$PROJECT_PACKAGES_DIR"),
            os.environ.get("PLATFORMIO_PACKAGES_DIR", ""),
            str(Path.home() / ".platformio" / "packages"),
        )
        if candidate and "$" not in candidate
    ]
    seen_roots = set(roots)
    for package_dir in package_dirs:
        if not package_dir.is_dir():
            continue
        for framework_libraries in sorted(package_dir.glob("framework-*/libraries")):
            if framework_libraries not in seen_roots:
                roots.append(framework_libraries)
                seen_roots.add(framework_libraries)
    return roots


def _generated_cpp_paths() -> list[str]:
    """Return generated and hardware-lib include paths needed by this profile."""
    paths = [str(project.settings.output_dir)]
    header = _include_operand_path(device.hardware_include)
    if header is None:
        return paths

    seen_paths = set(paths)
    pending = [header]
    seen_headers: set[Path] = set()
    search_roots = _platformio_header_search_roots()
    for _ in range(GENERATED_HEADER_DEPENDENCY_DEPTH):
        next_pending: list[Path] = []
        for pending_header in pending:
            if pending_header in seen_headers:
                continue
            seen_headers.add(pending_header)
            for root in search_roots:
                for location in _matching_header_locations(pending_header, root):
                    include_root = _include_root_for_header(pending_header, location)
                    if include_root not in seen_paths:
                        paths.append(include_root)
                        seen_paths.add(include_root)
                    next_pending.extend(_header_includes(location))
        pending = next_pending
    return paths


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
build_flags = lsh_static_config.raw_build_flags(project, device)
for define in lsh_static_config.merged_defines(project, device):
    if lsh_static_config.define_needs_escaped_build_flag(define):
        build_flags.append(lsh_static_config.render_escaped_build_flag_define(define))
        continue
    if define.value is None:
        cpp_defines.append(define.name)
    else:
        cpp_defines.append((define.name, define.value))

env.Append(
    CPPDEFINES=cpp_defines,
    CPPPATH=_generated_cpp_paths(),
    BUILD_FLAGS=build_flags,
)

sys.stdout.write(
    f"lsh-core: generated static config for {device_key} from {config_path.name}\n"
)
