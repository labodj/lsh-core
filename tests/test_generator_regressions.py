"""Regression cases for generated actions, project output and AVR field widths."""

from __future__ import annotations

import json
import re
import shutil
import subprocess
from typing import TYPE_CHECKING

import pytest

from tools import generate_lsh_static_config as gen
from tools import migrate_lsh_config as migration
from tools.lsh_static_config.action_bodies import render_majority_on, render_minority_on
from tools.lsh_static_config.click_actions import (
    render_direct_super_long_clicks,
    render_long_normal_body,
    render_run_network_click_fallback,
)
from tools.lsh_static_config.click_scan import (
    render_long_local_action,
    render_super_long_local_action,
)
from tools.lsh_static_config.cpp import render_lookup_function
from tools.lsh_static_config.profile import collect_static_profile_data
from tools.lsh_static_config.runtime_paths import render_indicator_case_body

if TYPE_CHECKING:
    from pathlib import Path

BASE = 'schema_version = 2\npreset = "arduino-generic/json"\n'
RELAYS = '[devices.demo.actuators.a]\npin="3"\n[devices.demo.actuators.b]\npin="4"\n'
BUTTON = '[devices.demo.buttons.key]\npin="2"\n'
CONFIG_ERROR = 2


def profile(root: Path, text: str) -> Path:
    """Write one self-contained public TOML profile."""
    path = root / "lsh_devices.toml"
    path.write_text(BASE + text, encoding="utf-8")
    return path


@pytest.mark.parametrize("value", ['"a"', '["a"]', '{target="a"}', '{group="room"}'])
def test_super_long_target_shorthand(tmp_path: Path, value: str) -> None:
    """Every target spelling selects precisely the named relay."""
    config = profile(
        tmp_path,
        RELAYS
        + '[devices.demo.groups.room]\ntarget="a"\n'
        + BUTTON
        + f"super_long={value}\n",
    )
    action = gen.parse_project(config).devices["demo"].clickables[0].super_long
    assert (action.click_type, action.targets) == ("SELECTIVE", ["a"])


@pytest.mark.parametrize("value", ["[]", "{targets=[]}", "{groups=[]}"])
def test_empty_super_long_never_means_all_off(tmp_path: Path, value: str) -> None:
    """An empty selection must never silently expand to every relay."""
    config = profile(tmp_path, RELAYS + BUTTON + f"super_long={value}\n")
    with pytest.raises(gen.ConfigError, match="at least one"):
        gen.parse_project(config)


def test_super_long_scan_and_network_fallback(tmp_path: Path) -> None:
    """Execute both generated paths and the uint8 boundary predicates on the host."""
    config = profile(
        tmp_path,
        RELAYS + BUTTON + 'super_long={target="a",network=true,fallback="local"}\n',
    )
    device = gen.parse_project(config).devices["demo"]
    data = collect_static_profile_data(device)
    source = [
        "#include <cassert>",
        "#include <cstdint>",
        "namespace constants { enum class ClickType { LONG, SUPER_LONG }; }",
        "bool a, b; bool actuator0_aActionSet(bool v){a=v;return true;}",
        "bool actuator1_bActionSet(bool v){b=v;return true;}",
        "bool turnOffUnprotectedActuators(){a=b=false;return true;}",
        *render_direct_super_long_clicks(device, data),
        *render_run_network_click_fallback(device),
        "int main(){a=b=true; uint8_t scanResultFlags=0;",
        "constexpr uint8_t CLICK_SCAN_STATE_CHANGED=1;",
        *render_super_long_local_action(device, data, 0),
        "assert(!a && b && scanResultFlags); a=b=true;",
        "assert(runNetworkClickFallback(0,constants::ClickType::SUPER_LONG));",
        "assert(!a && b);",
    ]
    for total in (1, 2, 3, 127, 128, 129, 254, 255):
        source.extend(
            (
                f"for(unsigned n=0;n<={total};++n){{const uint8_t actuatorsLongOn=n;",
                f"assert(({render_minority_on(total)})==(2*n<{total}));",
                f"assert(({render_majority_on('actuatorsLongOn', total)})"
                f"==(2*n>{total}));}}",
            )
        )
    cpp = tmp_path / "actions.cpp"
    cpp.write_text("\n".join([*source, "}"]), encoding="utf-8")
    compiler = shutil.which("c++")
    assert compiler is not None, (
        "A host C++ compiler is needed for the generated-action test"
    )
    binary = tmp_path / "actions"
    subprocess.run([compiler, "-std=c++17", str(cpp), "-o", str(binary)], check=True)  # noqa: S603
    subprocess.run([str(binary)], check=True)  # noqa: S603


@pytest.mark.parametrize(
    "alias", ["header", "schema", "guard", "lock", "symlink", "hardlink"]
)
def test_colliding_output_preserves_input(tmp_path: Path, alias: str) -> None:
    """Reject conflicting destinations before producing any files."""
    settings = {
        "header": 'user_config_header="lsh_devices.toml"',
        "guard": 'user_config_header="lsh-user.hpp"',
        "lock": 'id_lock_file="lsh_user_config.hpp"',
    }.get(alias, "")
    tail = (
        '[devices.demo]\nconfig_include="lsh_user.hpp"\n'
        if alias == "guard"
        else RELAYS
    )
    path = profile(tmp_path, '[generator]\noutput_dir="."\n' + settings + "\n" + tail)
    if alias == "symlink":
        (tmp_path / "lsh_user_config.hpp").symlink_to(path)
    if alias == "hardlink":
        (tmp_path / "lsh_user_config.hpp").hardlink_to(path)
    before = {p.name: p.read_bytes() for p in tmp_path.iterdir()}
    args = ["--write-vscode-schema", str(path)] if alias == "schema" else []
    assert gen.main([str(path), *args]) == CONFIG_ERROR
    assert {p.name: p.read_bytes() for p in tmp_path.iterdir()} == before


def test_schema_cannot_overwrite_lock_or_header(tmp_path: Path) -> None:
    """Refuse existing generated artifacts even when using an explicit schema path."""
    path = profile(tmp_path, RELAYS)
    assert gen.main([str(path)]) == 0
    for target in (
        tmp_path / "lsh_devices.lock.toml",
        tmp_path / "include/lsh_user_config.hpp",
    ):
        original = target.read_bytes()
        assert (
            gen.main([str(path), "--write-vscode-schema", str(target)]) == CONFIG_ERROR
        )
        assert target.read_bytes() == original


def test_generate_rechecks_path_aliases(tmp_path: Path) -> None:
    """A cached project must still protect the TOML when its output path changes."""
    path = profile(tmp_path, RELAYS)
    project = gen.parse_project(path)
    project.settings.output_dir.mkdir()
    (project.settings.output_dir / project.settings.user_config_header).hardlink_to(
        path
    )
    original = path.read_bytes()
    with pytest.raises(gen.ConfigError, match="aliases"):
        gen.generate(project, ["demo"])
    assert path.read_bytes() == original
    assert not project.settings.id_lock_file.exists()


def test_insert_before_locked_auto_id(tmp_path: Path) -> None:
    """An inserted resource preserves existing IDs and produces a stable lock."""
    path = profile(tmp_path, RELAYS)
    assert gen.main([str(path)]) == 0
    path = profile(tmp_path, '[devices.demo.actuators.new]\npin="5"\n' + RELAYS)
    assert gen.main([str(path)]) == 0
    assert gen.main([str(path), "--check"]) == 0
    actuators = gen.parse_project(path).devices["demo"].actuators
    assert {a.name: a.actuator_id for a in actuators} == {"new": 3, "a": 1, "b": 2}


def test_runtime_identity_collision_only_in_combined_export(tmp_path: Path) -> None:
    """Alternative profiles may describe the same controller individually."""
    path = profile(tmp_path, '[devices.a]\nname="same"\n[devices.b]\nname="same"\n')
    project = gen.parse_project(path)
    for key in project.devices:
        assert gen.build_stack_config(project, [key])["coordinator"]["systemConfig"][
            "devices"
        ] == [{"name": "same"}]
    with pytest.raises(gen.ConfigError, match=r"devices\.b\.name.*devices\.a\.name"):
        gen.build_stack_config(project, ["a", "b"])


@pytest.mark.parametrize(
    ("section", "field", "limit"),
    [
        ("timing", "actuator_debounce", 255),
        ("serial", "max_rx_payloads_per_loop", 255),
        ("timing", "scan_interval", 65535),
        ("serial", "max_rx_bytes_per_loop", 65535),
    ],
)
def test_field_widths_match_editor_schema(
    tmp_path: Path, section: str, field: str, limit: int
) -> None:
    """Expose and enforce actual upper limits at both project and device level."""
    schema = json.loads(gen.schema_json())
    assert schema["properties"][section]["properties"][field]["maximum"] == limit
    for prefix in ("", "devices.demo."):
        config = profile(tmp_path, f"[{prefix}{section}]\n{field}={limit}\n" + RELAYS)
        gen.parse_project(config)
        config = profile(
            tmp_path, f"[{prefix}{section}]\n{field}={limit + 1}\n" + RELAYS
        )
        with pytest.raises(gen.ConfigError, match=field):
            gen.parse_project(config)


def test_network_slot_limit(tmp_path: Path) -> None:
    """255 transactions fit; a second action on the last button would overflow."""
    buttons = 128
    settings = "".join(
        f'[devices.demo.buttons.b{i}]\npin="{i}"\nlong={{network=true}}\n'
        + ("super_long={network=true}\n" if i < buttons - 1 else "")
        for i in range(buttons)
    )
    gen.parse_project(profile(tmp_path, settings))
    with pytest.raises(gen.ConfigError, match="network_click_slots"):
        gen.parse_project(profile(tmp_path, settings + "super_long={network=true}\n"))


@pytest.mark.parametrize(
    "args",
    [
        ["--relays", "0"],
        ["--preset", "arduino-generic/json", "--buttons", "5"],
        ["--device-name", "two words"],
        ["--device-name", ""],
        ["--relays", "0", "--buttons", "0", "--indicators", "1"],
    ],
)
def test_invalid_scaffold_preserves_existing_project(
    tmp_path: Path, args: list[str]
) -> None:
    """Even --force validates the scaffold before replacing the current project."""
    config = profile(tmp_path, RELAYS)
    before = config.read_bytes()
    assert gen.main(["--init-config", str(config), "--force", *args]) == CONFIG_ERROR
    assert config.read_bytes() == before
    assert not (tmp_path / ".vscode").exists()


@pytest.mark.parametrize(
    "literal", ["250000UL", "0x3D090UL", "0750220u", "0b111101000010010000LL"]
)
def test_stack_export_unsigned_literals(tmp_path: Path, literal: str) -> None:
    """Decode C++ bases and suffixes consistently with the controller compiler."""
    path = profile(
        tmp_path, f'[advanced.defines]\nCONFIG_COM_SERIAL_BAUD="{literal}"\n' + RELAYS
    )
    project = gen.parse_project(path)
    assert "CONFIG_ARDCOM_SERIAL_BAUD=250000U" in gen.render_stack_report(
        project, ["demo"]
    )


def test_untranslatable_define_reports_its_name(tmp_path: Path) -> None:
    """Identify the advanced define that the stack cannot export."""
    path = profile(
        tmp_path, '[advanced.defines]\nCONFIG_COM_SERIAL_BAUD="F_CPU / 64"\n' + RELAYS
    )
    project = gen.parse_project(path)
    with pytest.raises(gen.ConfigError, match="CONFIG_COM_SERIAL_BAUD"):
        gen.render_stack_config_json(project, ["demo"])


def test_four_sparse_ids_avoid_avr_switch_table() -> None:
    """The four-relay Uno must not emit the SRAM-consuming sparse switch."""
    accessor = "\n".join(
        render_lookup_function("index", "id", {1: 0, 9: 1, 17: 2, 25: 3})
    )
    assert "switch" not in accessor
    for relay_id in (1, 9, 17, 25):
        assert f"id == {relay_id}U" in accessor


LEGACY = """[common]
hardware_include="Arduino.h"
[devices.demo]
[[devices.demo.actuators]]
name="a"
id=1
pin="3"
[[devices.demo.actuators]]
name="b"
id=2
pin="4"
[[devices.demo.clickables]]
name="key"
id=1
pin="2"
super_long={type="selective", network=true, fallback="local"}
"""


def test_migrated_empty_network_selection(tmp_path: Path) -> None:
    """Run the migrated timeout fallback with both physical outputs initially on."""
    path = tmp_path / "old.toml"
    path.write_text(LEGACY)
    assert migration.main([str(path), "--write"]) == 0
    device = gen.parse_project(path).devices["demo"]
    data = collect_static_profile_data(device)
    cpp = tmp_path / "migrated.cpp"
    cpp.write_text(
        "\n".join(
            [
                "#include <cstdint>",
                "namespace constants { enum class ClickType { LONG,SUPER_LONG }; }",
                "bool a=true,b=true; "
                "bool turnOffUnprotectedActuators(){a=b=false;return true;}",
                *render_direct_super_long_clicks(device, data),
                *render_run_network_click_fallback(device),
                "int main(){runNetworkClickFallback(0,"
                "constants::ClickType::SUPER_LONG);"
                "return !(a&&b);}",
            ]
        )
    )
    compiler = shutil.which("c++")
    assert compiler
    binary = tmp_path / "migrated"
    subprocess.run([compiler, "-std=c++17", str(cpp), "-o", str(binary)], check=True)  # noqa: S603
    subprocess.run([str(binary)], check=True)  # noqa: S603


@pytest.mark.parametrize(
    "invalid", [LEGACY.replace('name="b"', 'name="a"'), LEGACY.replace("id=2", "id=1")]
)
def test_invalid_migration_preserves_files(tmp_path: Path, invalid: str) -> None:
    """Both migration destinations survive malformed or ambiguous legacy resources."""
    path = tmp_path / "old.toml"
    path.write_text(invalid)
    other = tmp_path / "new.toml"
    other.write_bytes(b"existing")
    assert migration.main([str(path), "--write"]) == CONFIG_ERROR
    assert migration.main([str(path), "--output", str(other)]) == CONFIG_ERROR
    assert path.read_text() == invalid
    assert other.read_bytes() == b"existing"


def test_preflight_preserves_files(tmp_path: Path) -> None:
    """Foreign lockfiles and unreadable headers leave the entire output tree intact."""
    path = profile(tmp_path, RELAYS)
    project = gen.parse_project(path)
    lock = project.settings.id_lock_file
    for foreign in (
        b'[platformio]\nname="test"\n',
        b"[devices.obsolete.unknown]\na=1\n",
        b"",
    ):
        lock.write_bytes(foreign)
        assert gen.main([str(path)]) == CONFIG_ERROR
        with pytest.raises(gen.ConfigError):
            gen.generate(project, ["demo"])
        assert lock.read_bytes() == foreign
        assert not project.settings.output_dir.exists()
    lock.unlink()
    header = project.settings.output_dir / project.settings.user_config_header
    header.parent.mkdir()
    header.write_bytes(b"\xff")
    assert gen.main([str(path)]) == CONFIG_ERROR
    assert header.read_bytes() == b"\xff"
    assert list(header.parent.iterdir()) == [header]
    assert not lock.exists()
    header.write_text("// handmade")
    assert gen.main([str(path)]) == CONFIG_ERROR
    assert header.read_text() == "// handmade"
    assert not lock.exists()
    path.write_bytes(b"\xff")
    assert gen.main([str(path)]) == CONFIG_ERROR


def test_check_format_and_optional_schema(tmp_path: Path) -> None:
    """Checking a canonical file is read-only and opting out removes the editor hint."""
    path = tmp_path / "new.toml"
    assert gen.main(["--init-config", str(path), "--no-vscode-schema"]) == 0
    assert "#:schema" not in path.read_text()
    assert gen.main([str(path), "--format-config"]) == 0
    assert gen.main([str(path), "--check-format"]) == 0
    assert list(tmp_path.iterdir()) == [path]


@pytest.mark.parametrize(
    ("text", "error"),
    [
        ("[serial]\nrx_buffer_size=1\n" + RELAYS, "rx_buffer_size"),
        ("[serial]\ntx_buffer_size=1\n" + RELAYS, "tx_buffer_size"),
        (RELAYS + "auto_off_ms=4294967295\n", "auto_off"),
        (
            "[advanced.defines]\nCONFIG_CLICKABLE_LONG_CLICK_TIME_MS=1500\n" + RELAYS,
            "timing.long_click",
        ),
        (
            "[advanced]\n"
            'build_flags=["-D CONFIG_CLICKABLE_SUPER_LONG_CLICK_TIME_MS=1600"]\n'
            + RELAYS,
            "timing.super_long_click",
        ),
        (RELAYS.replace(".a]", ".OFF]") + BUTTON + 'super_long="off"\n', "ambiguous"),
        (
            RELAYS
            + 'protected=true\n[devices.demo.scenes.s]\ntoggle="b"\n'
            + BUTTON
            + 'super_long={scene="s"}\n',
            "protected",
        ),
    ],
)
def test_reject_silent_runtime_errors(tmp_path: Path, text: str, error: str) -> None:
    """Unsafe timer, buffer and action options fail before firmware generation."""
    with pytest.raises(gen.ConfigError, match=error):
        gen.parse_project(profile(tmp_path, text))


def test_maxi_alias_and_clock_boundary(tmp_path: Path) -> None:
    """Check the Maxi relay alias and an auto-off deadline across clock wrap."""
    path = profile(tmp_path, RELAYS)
    path.write_text(
        path.read_text()
        .replace("arduino-generic/json", "controllino-maxi/fast-json")
        .replace('"3"', '"R0"')
        .replace('"4"', '"22"')
    )
    with pytest.raises(gen.ConfigError, match="duplicates"):
        gen.parse_project(path)
    path = profile(tmp_path, RELAYS + "auto_off_ms=2147483647\n")
    timer = gen.parse_project(path).devices["demo"].actuators[1].auto_off_ms
    assert timer is not None
    mask = (1 << 32) - 1
    start = mask - timer + 500
    assert ((start + timer - 500) & mask) - start < timer
    assert (((start + timer + 500) & mask) - start) & mask >= timer


@pytest.mark.parametrize(
    ("timing", "valid"),
    [
        ("ping_interval=7000\nconnection_timeout=7300", True),
        ("ping_interval=20000", True),
        ("ping_interval=0", False),
        ("connection_timeout=9000", False),
    ],
)
def test_bridge_heartbeat_contract(tmp_path: Path, timing: str, *, valid: bool) -> None:
    """The exported heartbeat stays inside both endpoints' liveness window."""
    path = profile(tmp_path, "[timing]\n" + timing + "\n" + RELAYS)
    project = gen.parse_project(path)
    if valid:
        flags = gen.build_stack_config(project, ["demo"])["bridge"]["devices"]["demo"][
            "platformioBuildFlags"
        ]
        assert any("CONFIG_PING_INTERVAL_CONTROLLINO_MS=" in flag for flag in flags)
    else:
        with pytest.raises(gen.ConfigError, match="ping_interval"):
            gen.build_stack_config(project, ["demo"])


def test_long_pair_and_majority_indicator(tmp_path: Path) -> None:
    """The scan and fallback toggle identically; the indicator still counts both."""
    project = gen.parse_project(profile(tmp_path, RELAYS + BUTTON + 'long=["a","b"]\n'))
    device = project.devices["demo"]
    data = collect_static_profile_data(device)
    source = [
        "#include <cassert>",
        "#include <cstdint>",
        "struct Relay { bool value; bool getState(){return value;} };",
        "Relay actuator0_a, actuator1_b;",
        *[
            f"bool {name}ActionSet(bool v,uint32_t){{{name}.value=v;return true;}}"
            for name in ("actuator0_a", "actuator1_b")
        ],
        "bool majority(){",
        *render_indicator_case_body(device, [0, 1], "MAJORITY"),
        "}",
        "bool fallback(){",
        *render_long_normal_body(device, [0, 1]),
        "}",
        "bool scan(){uint8_t scanResultFlags=0; "
        "constexpr int CLICK_SCAN_STATE_CHANGED=1;",
        *render_long_local_action(device, data, 0),
        "return scanResultFlags;}",
        "int main(){bool (*paths[])()={scan,fallback}; for(auto run:paths){",
        "for(int mask=0;mask<4;++mask){",
        "actuator0_a.value=mask&1;actuator1_b.value=mask&2;",
        "assert(majority()==(mask==3));run();",
        "assert(actuator0_a.value==(mask==0)&&actuator1_b.value==(mask==0));}}}",
    ]
    cpp = tmp_path / "pair.cpp"
    cpp.write_text("\n".join(source))
    compiler = shutil.which("c++")
    assert compiler
    binary = tmp_path / "pair"
    subprocess.run([compiler, "-std=c++17", str(cpp), "-o", str(binary)], check=True)  # noqa: S603
    subprocess.run([str(binary)], check=True)  # noqa: S603


def test_action_schema_matches_click_kinds(tmp_path: Path) -> None:
    """The editor distinguishes actions and offers global super-long shorthand."""
    path = profile(tmp_path, RELAYS + BUTTON + 'super_long="all_off"\n')
    schema = json.loads(gen.project_schema_json(path))
    buttons = schema["properties"]["devices"]["properties"]["demo"]["properties"][
        "buttons"
    ]
    actions = buttons["properties"]["key"]["properties"]
    short = actions["short"]["oneOf"][2]["properties"]
    assert "network" not in short
    assert "off" not in short["action"]["enum"]
    assert actions["long"]["oneOf"][0] == {"const": False}
    compact = actions["super_long"]["oneOf"][1]["anyOf"]
    assert any(
        "pattern" in branch and re.fullmatch(branch["pattern"], "ALL_OFF")
        for branch in compact
    )
