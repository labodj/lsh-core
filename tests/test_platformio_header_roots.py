"""Keep generated hardware includes inside the active PlatformIO framework."""

from __future__ import annotations

import ast
from pathlib import Path
from types import SimpleNamespace
from typing import TYPE_CHECKING, cast

import pytest

if TYPE_CHECKING:
    from collections.abc import Callable


@pytest.mark.parametrize("enabled", [True, False])
def test_only_active_framework_libraries_are_searched(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, *, enabled: bool
) -> None:
    """An RP2040 host-test Arduino.h must never override the AVR core."""
    packages = tmp_path / ".platformio" / "packages"
    avr = packages / "framework-arduino-avr"
    foreign = packages / "framework-arduinopico" / "libraries" / "test" / "host"
    (avr / "libraries").mkdir(parents=True)
    foreign.mkdir(parents=True)
    (foreign / "Arduino.h").write_text("#error wrong framework\n")
    monkeypatch.setattr(Path, "home", lambda: tmp_path)
    monkeypatch.setenv("PLATFORMIO_PACKAGES_DIR", str(packages))
    platform = SimpleNamespace(
        frameworks={"arduino": {"package": "framework-arduino-avr"}},
        get_package_dir=lambda _name: str(avr),
    )
    variables = {
        "$PROJECT_LIBDEPS_DIR": str(tmp_path / "libdeps"),
        "$PROJECT_PACKAGES_DIR": str(packages),
        "$PIOENV": "uno",
    }
    env = SimpleNamespace(
        subst=lambda key: variables[key],
        GetProjectOption=lambda _name: ["arduino"] if enabled else [],
        PioPlatform=lambda: platform,
    )
    source = Path(__file__).parents[1] / "tools/platformio_lsh_static_config.py"
    tree = ast.parse(source.read_text())
    function = next(
        node
        for node in tree.body
        if isinstance(node, ast.FunctionDef)
        and node.name == "_platformio_header_search_roots"
    )
    namespace: dict[str, object] = {"Path": Path, "env": env}
    # The SCons hook executes a build at import; isolate its actual function.
    exec(  # noqa: S102
        compile(ast.Module(body=[function], type_ignores=[]), str(source), "exec"),
        namespace,
    )
    roots = cast("Callable[[], list[Path]]", namespace[function.name])()
    expected = [tmp_path / "libdeps" / "uno"]
    if enabled:
        expected.append(avr / "libraries")
    assert roots == expected
