"""Regression tests for the TOML-backed static configuration generator."""

from __future__ import annotations

import json
import tempfile
import textwrap
from dataclasses import dataclass
from pathlib import Path

import pytest

from tools import generate_lsh_static_config as gen
from tools.lsh_static_config.legacy_migration import migrate_file

DEFAULT_ACTUATOR = """
[devices.panel.actuators.relay]
id = 1
pin = "6"
"""

DEFAULT_CLICKABLE = """
[devices.panel.buttons.button]
id = 1
pin = "7"
short = "relay"
"""


@dataclass(frozen=True)
class ProfileParts:
    """Override points for a compact generated TOML fixture."""

    controller_fields: str = ""
    project_sections: str = ""
    device_fields: str = ""
    extra_sections: str = ""
    actuators: str = DEFAULT_ACTUATOR
    clickables: str = DEFAULT_CLICKABLE
    indicators: str = ""


def write_config(directory: Path, toml_text: str) -> Path:
    """Write a TOML fixture exactly like a user project would."""
    path = directory / "lsh_devices.toml"
    path.write_text(textwrap.dedent(toml_text).strip() + "\n", encoding="utf-8")
    return path


def minimal_profile(parts: ProfileParts | None = None) -> str:
    """Build a small valid profile, with targeted override points."""
    profile = parts if parts is not None else ProfileParts()
    return f"""
    schema_version = 2
    preset = "arduino-generic/json"

    [generator]
    output_dir = "out"
    config_dir = "generated"
    user_config_header = "lsh_user_config.hpp"

    [controller]
    debug_serial = "Serial"
    bridge_serial = "Serial1"
    {profile.controller_fields}

    {profile.project_sections}

    [devices.panel]
    name = "Panel"
    {profile.device_fields}

    {profile.extra_sections}
    {profile.actuators}
    {profile.clickables}
    {profile.indicators}
    """


def render_define(define: gen.DefineValue) -> str:
    """Render one PlatformIO define as the CLI prints it."""
    if define.value is None:
        return define.name
    return f"{define.name}={define.value}"


def assert_config_error_contains(toml_text: str, expected: str) -> None:
    """Assert that a malformed profile fails with a useful message."""
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(Path(tmpdir), toml_text)
        with pytest.raises(gen.ConfigError) as raised:
            gen.parse_project(config_path)
    assert expected in str(raised.value)


def test_generates_sparse_static_profile_without_runtime_tables() -> None:
    """Sparse user IDs and rich click options render into static C++."""
    actuators = """
    [devices.panel.actuators.relay_a]
    id = 1
    pin = "6"
    protected = true

    [devices.panel.actuators.relay_b]
    id = 9
    pin = "8"
    default = true
    auto_off = "10m"
    """
    clickables = """
    [devices.panel.buttons.button_a]
    id = 2
    pin = "7"
    short = ["relay_a", "relay_b"]
    long = { targets = ["relay_b"], action = "off", after = "900ms" }

    [devices.panel.buttons.button_b]
    id = 9
    pin = "9"
    short = "relay_a"
    long = { network = true, fallback = "do_nothing" }
    """
    indicators = """
    [devices.panel.indicators.led_a]
    pin = "10"
    when = { all = ["relay_a", "relay_b"] }
    """

    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(
            Path(tmpdir),
            minimal_profile(
                ProfileParts(
                    actuators=actuators,
                    clickables=clickables,
                    indicators=indicators,
                ),
            ),
        )
        project = gen.parse_project(config_path)
        files = gen.generated_files(project, ["panel"])

    static_header = next(
        content
        for path, content in files.items()
        if path.name == "panel_static_config.hpp"
    )
    assert "#define LSH_STATIC_CONFIG_MAX_ACTUATOR_ID 9" in static_header
    assert "#define LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS 1" in static_header
    assert "actuator0_relay_a.setIndex(0U);" in static_header
    assert "Actuators::actuators[0U] = &actuator0_relay_a;" in static_header
    assert "DETAILS_JSON_PAYLOAD" in static_header
    assert "DETAILS_MSGPACK_PAYLOAD" in static_header
    assert "button0_button_a.clickDetection<" in static_header
    assert "900U, 1000U" in static_header
    assert "getShortClickActuatorLinkCount" not in static_header
    assert "auto getShortClickActuatorLink(uint8_t" not in static_header
    assert "case 0U:" in static_header
    assert "actuator0_relay_aActionToggle(actionNow)" in static_header
    assert "actuator1_relay_bActionToggle(actionNow)" in static_header
    assert "NetworkClicks::request(1U, constants::ClickType::LONG)" in static_header
    assert "setClickableLong" not in static_header
    assert (
        "auto computeIndicatorState(uint8_t indicatorIndex) noexcept -> bool"
        in static_header
    )
    assert (
        "return actuator0_relay_a.getState() && actuator1_relay_b.getState();"
        in static_header
    )


def test_public_schema_v2_keeps_simple_profiles_readable() -> None:
    """Schema v2 exposes presets, named resources, pin aliases and action words."""
    explicit_wall_id = 9
    explicit_long_click_ms = 900
    toml_text = """
    schema_version = 2
    preset = "controllino-maxi/fast-msgpack"

    [generator]
    output_dir = "out"
    config_dir = "generated"

    [controller]
    debug_serial = "Serial"
    bridge_serial = "Serial2"
    disable_rtc = true
    disable_eth = true

    [timing]
    actuator_debounce = "0ms"
    button_debounce = "8ms"
    long_click = "450ms"
    super_long_click = "1200ms"

    [serial]
    bridge_baud = 500000
    flush_after_send = false
    rx_buffer_size = 256

    [features]
    aggressive_constexpr_ctors = true
    etl_profile_override_header = "lsh_etl_profile_override.h"

    [devices.panel]
    name = "Panel"

    [devices.panel.actuators.ceiling]
    pin = "R0"
    protected = true

    [devices.panel.actuators.wall]
    id = 9
    pin = "R1"
    default = true
    auto_off = "10m"

    [devices.panel.buttons.door]
    pin = "A0"
    short = "ceiling"
    long = { after = "900ms", action = "off", target = "wall" }
    super_long = { action = "all_off" }

    [devices.panel.indicators.main_led]
    pin = "D0"
    when = { all = ["ceiling", "wall"] }
    """

    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(Path(tmpdir), toml_text)
        project = gen.parse_project(config_path)
        files = gen.generated_files(project, ["panel"])

    device = project.devices["panel"]
    defines = [render_define(define) for define in gen.merged_defines(project, device)]
    assert device.hardware_include == "<Controllino.h>"
    assert device.com_serial == "&Serial2"
    assert device.disable_rtc is True
    assert device.disable_eth is True
    assert device.actuators[0].name == "ceiling"
    assert device.actuators[0].actuator_id == 1
    assert device.actuators[0].pin == "CONTROLLINO_R0"
    assert device.actuators[1].actuator_id == explicit_wall_id
    assert device.actuators[1].default_state is True
    assert device.clickables[0].pin == "CONTROLLINO_A0"
    assert device.clickables[0].long.click_type == "OFF_ONLY"
    assert device.clickables[0].long.time_ms == explicit_long_click_ms
    assert device.indicators[0].mode == "ALL"
    assert "CONFIG_MSG_PACK" in defines
    assert "CONFIG_USE_FAST_CLICKABLES" in defines
    assert "CONFIG_ACTUATOR_DEBOUNCE_TIME_MS=0" in defines
    assert "CONFIG_CLICKABLE_DEBOUNCE_TIME_MS=8" in defines
    assert "CONFIG_COM_SERIAL_BAUD=500000" in defines
    assert "CONFIG_COM_SERIAL_FLUSH_AFTER_SEND=0" in defines
    assert "LSH_ENABLE_AGGRESSIVE_CONSTEXPR_CTORS" in defines
    assert 'LSH_ETL_PROFILE_OVERRIDE_HEADER="lsh_etl_profile_override.h"' in defines
    assert gen.raw_build_flags(project, device) == ["-D SERIAL_RX_BUFFER_SIZE=256"]

    static_header = next(
        content
        for path, content in files.items()
        if path.name == "panel_static_config.hpp"
    )
    assert "LSH_ACTUATOR(actuator0_ceiling, CONTROLLINO_R0);" in static_header
    assert (
        "Actuator actuator1_wall(::lsh::core::PinTag<(CONTROLLINO_R1)>{}, true);"
        in static_header
    )
    assert "button0_door.clickDetection<" in static_header
    assert "900U, 1200U" in static_header
    assert (
        "return actuator0_ceiling.getState() && actuator1_wall.getState();"
        in static_header
    )


def test_groups_scenes_pulse_and_interlocks_are_static() -> None:
    """Declarative QoL aliases lower into direct generated actuator wrappers."""
    pulse_ms = 300
    actuators = """
    [devices.panel.actuators.relay_a]
    id = 1
    pin = "6"
    interlock = "relay_b"

    [devices.panel.actuators.relay_b]
    id = 2
    pin = "7"
    interlock = "relay_a"

    [devices.panel.actuators.door_strike]
    id = 3
    pin = "8"
    pulse = "300ms"
    """
    extra_sections = """
    [devices.panel.groups.main_lights]
    targets = ["relay_a", "relay_b"]

    [devices.panel.scenes.night]
    off = "main_lights"
    on = "door_strike"
    """
    clickables = """
    [devices.panel.buttons.scene_button]
    id = 1
    pin = "9"
    short = { group = "main_lights" }
    long = { scene = "night", after = "500ms" }
    super_long = { action = "off", group = "main_lights" }
    """

    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(
            Path(tmpdir),
            minimal_profile(
                ProfileParts(
                    extra_sections=extra_sections,
                    actuators=actuators,
                    clickables=clickables,
                ),
            ),
        )
        project = gen.parse_project(config_path)
        files = gen.generated_files(project, ["panel"])

    device = project.devices["panel"]
    assert device.actuators[0].interlock_targets == ["relay_b"]
    assert device.actuators[2].pulse_ms == pulse_ms
    assert device.clickables[0].short_targets == ["relay_a", "relay_b"]
    assert [step.operation for step in device.clickables[0].long.steps] == [
        "OFF",
        "ON",
    ]

    static_header = next(
        content
        for path, content in files.items()
        if path.name == "panel_static_config.hpp"
    )
    assert "#define LSH_STATIC_CONFIG_PULSE_ACTUATORS 1" in static_header
    assert (
        "static uint16_t pulseRemaining_ms[CONFIG_PULSE_STORAGE_CAPACITY]"
        in static_header
    )
    assert "static uint8_t activePulseActuators = 0U;" in static_header
    assert (
        "auto checkPulseTimers(uint16_t elapsed_ms) noexcept -> bool" in static_header
    )
    assert "actuator1_relay_bActionSet(false, actionNow);" in static_header
    assert "actuator0_relay_aActionSet(false, actionNow);" in static_header
    assert (
        "static_assert(300U >= constants::timings::ACTUATOR_DEBOUNCE_TIME_MS"
        in static_header
    )
    assert "pulseRemaining_ms[0U] = 300U;" in static_header
    assert "actuator2_door_strikeActionSet(true, actionNow)" in static_header


def test_include_operand_defines_are_escaped_for_platformio_build_flags() -> None:
    """Header operands keep their delimiters when emitted through build flags."""
    quoted = gen.DefineValue(
        "LSH_ETL_PROFILE_OVERRIDE_HEADER",
        '"lsh_etl_profile_override.h"',
    )
    angled = gen.DefineValue("LSH_TEST_HEADER", "<lsh/test.hpp>")
    integer = gen.DefineValue("CONFIG_DEBUG_SERIAL_BAUD", "500000")

    assert gen.define_needs_escaped_build_flag(quoted) is True
    assert gen.define_needs_escaped_build_flag(angled) is True
    assert gen.define_needs_escaped_build_flag(integer) is False
    assert (
        gen.render_escaped_build_flag_define(quoted)
        == r"-DLSH_ETL_PROFILE_OVERRIDE_HEADER=\"lsh_etl_profile_override.h\""
    )
    assert (
        gen.render_escaped_build_flag_define(angled)
        == r"-DLSH_TEST_HEADER=\<lsh/test.hpp\>"
    )


def test_lockfile_keeps_automatic_ids_stable() -> None:
    """Omitted public IDs are restored from lsh_devices.lock.toml."""
    locked_relay_id = 7
    locked_button_id = 9
    toml_text = minimal_profile(
        ProfileParts(
            actuators="""
            [devices.panel.actuators.relay]
            pin = "6"

            [devices.panel.actuators.extra]
            pin = "8"
            """,
            clickables="""
            [devices.panel.buttons.button]
            pin = "7"
            short = "relay"
            """,
        )
    )

    with tempfile.TemporaryDirectory() as tmpdir:
        directory = Path(tmpdir)
        (directory / "lsh_devices.lock.toml").write_text(
            textwrap.dedent(
                """
                schema_version = 1

                [devices.panel.actuators]
                relay = 7

                [devices.panel.buttons]
                button = 9
                """
            ).strip()
            + "\n",
            encoding="utf-8",
        )
        project = gen.parse_project(write_config(directory, toml_text))

    device = project.devices["panel"]
    assert device.actuators[0].actuator_id == locked_relay_id
    assert device.actuators[1].actuator_id == 1
    assert device.clickables[0].clickable_id == locked_button_id
    assert "relay = 7" in project.id_lock_content
    assert "extra = 1" in project.id_lock_content
    assert "button = 9" in project.id_lock_content
    assert project.auto_id_paths == [
        "devices.panel.actuators.relay.id",
        "devices.panel.actuators.extra.id",
        "devices.panel.buttons.button.id",
    ]


def test_generation_writes_lockfile_and_check_detects_stale_lock() -> None:
    """Generation owns the ID lockfile beside generated headers."""
    with tempfile.TemporaryDirectory() as tmpdir:
        directory = Path(tmpdir)
        config_path = write_config(
            directory,
            minimal_profile(
                ProfileParts(
                    actuators="""
                    [devices.panel.actuators.relay]
                    pin = "6"
                    """,
                    clickables="""
                    [devices.panel.buttons.button]
                    pin = "7"
                    short = "relay"
                    """,
                )
            ),
        )
        project = gen.parse_project(config_path)
        assert gen.generate(project, ["panel"], check=True) == 1
        assert gen.generate(project, ["panel"], check=False) == 0
        assert (directory / "lsh_devices.lock.toml").exists()
        assert gen.generate(gen.parse_project(config_path), ["panel"], check=True) == 0


def test_doctor_schema_and_formatter_are_public_tooling() -> None:
    """Adoption tooling is available without touching generated C++."""
    with tempfile.TemporaryDirectory() as tmpdir:
        directory = Path(tmpdir)
        config_path = write_config(
            directory,
            minimal_profile(
                ProfileParts(
                    actuators="""
                    [devices.panel.actuators.relay]
                    pin = "6"
                    """,
                    clickables="""
                    [devices.panel.buttons.button]
                    pin = "7"
                    short = "relay"
                    """,
                )
            ),
        )
        config_path.write_text(
            "#:schema docs/lsh_devices.schema.json\n"
            + config_path.read_text(encoding="utf-8"),
            encoding="utf-8",
        )
        project = gen.parse_project(config_path)
        diagnostics = gen.diagnose_project(project)

        assert any("automatic IDs are used" in item.message for item in diagnostics)
        assert '"schema_version"' in gen.schema_json()
        assert '"controllino-maxi/fast-msgpack"' in gen.schema_json()
        assert gen.format_config_file(config_path, check=True) is True
        assert gen.format_config_file(config_path, check=False) is True
        assert gen.format_config_file(config_path, check=True) is False
        assert config_path.read_text(encoding="utf-8").startswith(
            "#:schema docs/lsh_devices.schema.json\n"
        )


def test_project_schema_suggests_local_resource_names() -> None:
    """Project-specific schemas expose local names as editor enum suggestions."""
    extra_sections = """
    [devices.panel.groups.room]
    targets = ["relay"]

    [devices.panel.scenes.shutdown]
    off = "room"
    """
    clickables = """
    [devices.panel.buttons.button]
    pin = "7"
    short = { group = "room" }
    long = { scene = "shutdown", after = "900ms" }
    """

    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(
            Path(tmpdir),
            minimal_profile(
                ProfileParts(
                    extra_sections=extra_sections,
                    clickables=clickables,
                ),
            ),
        )
        project_schema = json.loads(gen.project_schema_json(config_path))

    device_schema = project_schema["properties"]["devices"]["properties"]["panel"]
    button_schema = device_schema["properties"]["buttons"]["properties"]["button"]
    click_object = button_schema["properties"]["short"]["oneOf"][2]
    assert click_object["properties"]["target"]["enum"] == ["relay"]
    assert click_object["properties"]["group"]["enum"] == ["room"]
    assert click_object["properties"]["scene"]["enum"] == ["shutdown"]


def test_guided_scaffold_writes_valid_toml_and_local_schema() -> None:
    """The starter scaffold is immediately parseable and editor-ready."""
    with tempfile.TemporaryDirectory() as tmpdir:
        directory = Path(tmpdir)
        config_path = gen.write_scaffold(
            directory / "lsh_devices.toml",
            gen.ScaffoldOptions(
                preset="arduino-generic/json",
                device_key="panel",
                relays=2,
                buttons=2,
                indicators=1,
            ),
        )
        project = gen.parse_project(config_path)
        schema_path = directory / ".vscode" / "lsh_devices.schema.json"

        assert schema_path.exists()
        assert project.devices["panel"].actuators[0].name == "relay1"
        assert project.devices["panel"].clickables[0].short_targets == ["relay1"]
        assert '"relay1"' in schema_path.read_text(encoding="utf-8")


def test_formatter_compacts_simple_button_actions() -> None:
    """Formatting keeps common button actions compact and readable."""
    clickables = """
    [devices.panel.buttons.button]
    pin = "7"

    [devices.panel.buttons.button.short]
    targets = ["relay"]

    [devices.panel.buttons.button.long]
    targets = ["relay"]

    [devices.panel.buttons.button.super_long]
    action = "all_off"
    """

    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(
            Path(tmpdir),
            minimal_profile(ProfileParts(clickables=clickables)),
        )

        assert gen.format_config_file(config_path, check=False) is True

        formatted = config_path.read_text(encoding="utf-8")
        assert "[devices]" not in formatted
        assert "[devices.panel.buttons]" not in formatted
        assert 'short = "relay"' in formatted
        assert 'long = "relay"' in formatted
        assert 'super_long = { action = "all_off" }' in formatted


def test_migration_tool_outputs_schema_v2_without_enabling_legacy_parser() -> None:
    """The explicit migrator converts old syntax, but parse_project stays v2-only."""
    legacy_toml = """
    [common]
    hardware_include = "Arduino.h"
    debug_serial = "Serial"
    com_serial = "Serial1"

    [common.defines]
    CONFIG_MSG_PACK = true
    CONFIG_USE_FAST_ACTUATORS = true

    [devices.panel]
    name = "Panel"

    [[devices.panel.actuators]]
    name = "relay"
    id = 1
    pin = "6"
    default_state = true

    [[devices.panel.clickables]]
    name = "button"
    id = 1
    pin = "7"
    short = ["relay"]
    long = { targets = ["relay"], type = "off_only", time = "900ms" }
    super_long = { network = true, fallback = "do-nothing" }
    """

    with tempfile.TemporaryDirectory() as tmpdir:
        directory = Path(tmpdir)
        legacy_path = write_config(directory, legacy_toml)
        assert_config_error_contains(legacy_toml, "schema_version must be 2")
        migrated = migrate_file(legacy_path)
        migrated_path = write_config(directory, migrated)
        project = gen.parse_project(migrated_path)

    assert "schema_version = 2" in migrated
    assert "[devices.panel.actuators.relay]" in migrated
    assert 'action = "off"' in migrated
    assert 'fallback = "do_nothing"' in migrated
    assert project.devices["panel"].clickables[0].long.click_type == "OFF_ONLY"


def test_minimal_generated_code_matches_golden_snippets() -> None:
    """A small golden file catches accidental static-code shape drift."""
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(Path(tmpdir), minimal_profile())
        project = gen.parse_project(config_path)
        files = gen.generated_files(project, ["panel"])

    static_header = next(
        content
        for path, content in files.items()
        if path.name == "panel_static_config.hpp"
    )
    actual = [
        line.strip()
        for line in static_header.splitlines()
        if any(
            snippet in line
            for snippet in (
                "#define LSH_STATIC_CONFIG_CLICKABLES",
                "#define LSH_STATIC_CONFIG_ACTUATORS",
                "#define LSH_STATIC_CONFIG_INDICATORS",
                "LSH_ACTUATOR(actuator0_relay, 6);",
                "return actuator0_relayActionToggle();",
                "button0_button.clickDetection<",
                "if (actuator0_relayActionToggle())",
            )
        )
    ]
    expected_path = (
        Path(__file__).parent / "golden" / ("minimal_schema_v2_static_snippets.txt")
    )
    expected = expected_path.read_text(encoding="utf-8").splitlines()
    assert actual == expected


@pytest.mark.parametrize(
    ("toml_text", "expected"),
    [
        (
            """
            schema_version = 2
            preset = "controllino-maxi/fast-msgpack"

            [devices.panel.actuators.relay]
            pni = "R0"
            """,
            "pni is not a supported schema v2 field",
        ),
        (
            """
            schema_version = 2
            preset = "controllino-maxi/fast-msgpack"

            [devices.panel.actuators.relay]
            pin = "R0"

            [devices.panel.buttons.button]
            pin = "A0"
            short = { target = "relay", targets = ["relay"] }
            """,
            "must use only one of target/targets",
        ),
        (
            """
            schema_version = 2
            preset = "controllino-maxi/fast-msgpack"

            [devices.panel.actuators.relay]
            pin = "R0"

            [devices.panel.buttons.button]
            pin = "A0"
            short = "relay"
            super_long = { action = "all_off", targets = ["relay"] }
            """,
            'action="all_off" cannot also list targets',
        ),
    ],
)
def test_public_schema_v2_rejects_ambiguous_profiles(
    toml_text: str,
    expected: str,
) -> None:
    """The public schema fails fast on typo-prone or ambiguous declarations."""
    assert_config_error_contains(toml_text, expected)


def test_merges_defines_raw_flags_and_environment_names() -> None:
    """Common and device settings resolve exactly as PlatformIO consumes them."""
    extra_sections = """
    [advanced]
    build_flags = ["-flto"]

    [advanced.defines]
    CONFIG_MSG_PACK = true
    CONFIG_USE_FAST_ACTUATORS = true
    CONFIG_CLICKABLE_DEBOUNCE_TIME_MS = 20
    CONFIG_DISABLED_BY_DEFAULT = false

    [devices.panel.advanced]
    build_flags = ["-Wl,--gc-sections"]

    [devices.panel.advanced.defines]
    CONFIG_MSG_PACK = false
    CONFIG_CLICKABLE_DEBOUNCE_TIME_MS = 30
    CONFIG_USE_FAST_CLICKABLES = true
    CONFIG_CUSTOM_LABEL = "PANEL_A"
    """

    # Human-edited profiles commonly group define tables apart from resource
    # arrays; the merge must stay deterministic regardless of that order.
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(
            Path(tmpdir),
            minimal_profile(
                ProfileParts(
                    project_sections=extra_sections,
                ),
            ),
        )
        project = gen.parse_project(config_path)

    device = project.devices["panel"]
    defines = [render_define(define) for define in gen.merged_defines(project, device)]
    assert defines[0] == "LSH_BUILD_PANEL"
    assert "CONFIG_MSG_PACK" not in defines
    assert "CONFIG_DISABLED_BY_DEFAULT" not in defines
    assert "CONFIG_USE_FAST_ACTUATORS" in defines
    assert "CONFIG_USE_FAST_CLICKABLES" in defines
    assert "CONFIG_CLICKABLE_DEBOUNCE_TIME_MS=30" in defines
    assert "CONFIG_CUSTOM_LABEL=PANEL_A" in defines
    assert gen.raw_build_flags(project, device) == ["-flto", "-Wl,--gc-sections"]
    assert gen.infer_device_from_env(project, "panel_release") == "panel"


def test_custom_generated_header_names_emit_include_selector_defines() -> None:
    """Custom generated router names stay buildable through PlatformIO defines."""
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(
            Path(tmpdir),
            minimal_profile().replace(
                'user_config_header = "lsh_user_config.hpp"',
                'user_config_header = "custom_user_config.hpp"\n'
                'static_config_router_header = "custom_static_router.hpp"',
            ),
        )
        project = gen.parse_project(config_path)

    device = project.devices["panel"]
    defines = [render_define(define) for define in gen.merged_defines(project, device)]
    assert 'LSH_CUSTOM_USER_CONFIG_HEADER="custom_user_config.hpp"' in defines
    assert (
        'LSH_CUSTOM_STATIC_CONFIG_ROUTER_HEADER="custom_static_router.hpp"' in defines
    )


def test_renders_extreme_static_profiles() -> None:
    """Edge profiles cover zero resources, sparse IDs and every action family."""
    actuators = """
    [devices.panel.actuators.relay_a]
    id = 1
    pin = "6"
    protected = true

    [devices.panel.actuators.relay_b]
    id = 17
    pin = "7"
    default = true
    auto_off_ms = 255

    [devices.panel.actuators.relay_c]
    id = 255
    pin = "8"
    auto_off = "65s"
    """
    clickables = """
    [devices.panel.buttons.button_network]
    id = 250
    pin = "9"
    short = false
    long = { network = true, fallback = "do_nothing", after = "401ms" }
    super_long = { network = true, fallback = "local", time_ms = 1200 }

    [devices.panel.buttons.button_local]
    id = 2
    pin = "10"
    short = ["relay_a", "relay_c"]
    long = { targets = ["relay_b", "relay_c"], action = "off" }
    super_long = false
    """
    indicators = """
    [devices.panel.indicators.led_majority]
    pin = "11"
    when = { majority = ["relay_a", "relay_b", "relay_c"] }
    """

    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(
            Path(tmpdir),
            minimal_profile(
                ProfileParts(
                    device_fields="""
                    [devices.panel.features]
                    codec = "msgpack"
                    fast_io = true

                    [devices.panel.timing]
                    actuator_debounce = "0ms"
                    """,
                    actuators=actuators,
                    clickables=clickables,
                    indicators=indicators,
                ),
            ),
        )
        project = gen.parse_project(config_path)
        files = gen.generated_files(project, ["panel"])

    static_header = next(
        content
        for path, content in files.items()
        if path.name == "panel_static_config.hpp"
    )
    assert "#define LSH_STATIC_CONFIG_MAX_ACTUATOR_ID 255" in static_header
    assert "#define LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS 2" in static_header
    assert "button0_button_network.clickDetection<" in static_header
    assert "constants::clickDetection::makeFlags(false, true, true)" in static_header
    assert "setClickable" not in static_header
    assert "NoNetworkClickType::DO_NOTHING" not in static_header
    assert "const uint8_t totalControlledActuatorsOn" in static_header
    assert (
        "return static_cast<uint8_t>(totalControlledActuatorsOn << 1U) > 3U;"
        in static_header
    )
    assert "case 0U:" in static_header
    assert "case 1U:" in static_header


def test_allows_a_device_with_zero_resources() -> None:
    """A telemetry-only or placeholder profile should not need dummy objects."""
    empty_profile = minimal_profile(
        ProfileParts(
            actuators="",
            clickables="",
            indicators="",
        ),
    )

    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(Path(tmpdir), empty_profile)
        project = gen.parse_project(config_path)
        files = gen.generated_files(project, ["panel"])

    static_header = next(
        content
        for path, content in files.items()
        if path.name == "panel_static_config.hpp"
    )
    assert "#define LSH_STATIC_CONFIG_ACTUATORS 0" in static_header
    assert "#define LSH_STATIC_CONFIG_CLICKABLES 0" in static_header
    assert "#define LSH_STATIC_CONFIG_INDICATORS 0" in static_header
    assert "static_cast<void>(clickableIndex);" in static_header


def test_all_options_catalog_renders_every_profile() -> None:
    """The public exhaustive TOML catalog stays accepted by the generator."""
    config_path = (
        Path(__file__).parents[1] / "examples" / "all-options-toml" / "lsh_devices.toml"
    )
    project = gen.parse_project(config_path)
    assert set(project.devices) == {
        "maximal_panel",
        "no_network_dense",
        "empty_profile",
    }

    files = gen.generated_files(project, list(project.devices))
    rendered = "\n".join(files.values())
    assert "LSH_BUILD_ALL_OPTIONS_MAXIMAL" in rendered
    assert "LSH_STATIC_CONFIG_ACTIVE_NETWORK_CLICKS" in rendered
    assert "lsh_all_options_static_router.hpp" in {path.name for path in files}
    assert "custom/maximal_panel_static_config.hpp" in rendered


def test_cookbook_example_renders() -> None:
    """The official cookbook TOML remains a valid copyable profile."""
    config_path = (
        Path(__file__).parents[1] / "examples" / "cookbook" / "lsh_devices.toml"
    )
    project = gen.parse_project(config_path)
    files = gen.generated_files(project, ["kitchen"])

    rendered = "\n".join(files.values())
    assert "LSH_STATIC_CONFIG_PULSE_ACTUATORS 1" in rendered
    assert "button1_worktop.clickDetection<" in rendered
    assert "actuator4_blind_upActionSet(false" in rendered


def test_resource_names_are_independent_across_families() -> None:
    """Actuators, buttons and indicators may share one logical TOML name."""
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(
            Path(tmpdir),
            minimal_profile(
                ProfileParts(
                    actuators="""
                    [devices.panel.actuators.bagno]
                    id = 1
                    pin = "6"
                    """,
                    clickables="""
                    [devices.panel.buttons.bagno]
                    id = 1
                    pin = "7"
                    short = "bagno"
                    """,
                    indicators="""
                    [devices.panel.indicators.bagno]
                    pin = "8"
                    when = "bagno"
                    """,
                ),
            ),
        )
        project = gen.parse_project(config_path)
        files = gen.generated_files(project, ["panel"])

    static_header = next(
        content
        for path, content in files.items()
        if path.name == "panel_static_config.hpp"
    )
    assert "LSH_ACTUATOR(actuator0_bagno, 6);" in static_header
    assert "LSH_BUTTON(button0_bagno, 7);" in static_header
    assert "LSH_INDICATOR(indicator0_bagno, 8);" in static_header
    assert "button0_bagno.clickDetection<" in static_header
    assert (
        "indicator0_bagno.applyComputedState(actuator0_bagno.getState());"
        in static_header
    )


@pytest.mark.parametrize(
    ("toml_text", "expected"),
    [
        (
            minimal_profile(
                ProfileParts(
                    actuators="""
                    [devices.panel.actuators.relay_a]
                    id = 1
                    pin = "6"

                    [devices.panel.actuators.relay_b]
                    id = 1
                    pin = "7"
                    """,
                ),
            ),
            "devices.panel.actuators.relay_b.id duplicates another ID",
        ),
        (
            minimal_profile(
                ProfileParts(
                    clickables=DEFAULT_CLICKABLE.replace('"relay"', '"missing"'),
                ),
            ),
            "references unknown actuator 'missing'",
        ),
        (
            minimal_profile(
                ProfileParts(
                    clickables=DEFAULT_CLICKABLE.replace(
                        'short = "relay"',
                        'short = { enabled = false, targets = ["relay"] }',
                    ),
                ),
            ),
            "has targets but is disabled",
        ),
        (
            minimal_profile(
                ProfileParts(
                    clickables=DEFAULT_CLICKABLE.replace(
                        'short = "relay"',
                        'short = ["relay", "relay"]',
                    ),
                ),
            ),
            "references actuator 'relay' more than once",
        ),
        (
            minimal_profile(
                ProfileParts(
                    clickables=DEFAULT_CLICKABLE.replace(
                        'short = "relay"',
                        "long = true",
                    ),
                ),
            ),
            "long clicks need local targets or network=true",
        ),
        (
            minimal_profile(
                ProfileParts(
                    extra_sections="""
                    [devices.panel.advanced.defines]
                    LSH_NETWORK_CLICKS = false
                    """,
                    clickables=DEFAULT_CLICKABLE.replace(
                        'short = "relay"',
                        "long = { network = true }",
                    ),
                ),
            ),
            "LSH_NETWORK_CLICKS is no longer supported",
        ),
        (
            minimal_profile(
                ProfileParts(
                    extra_sections="""
                    [devices.panel.advanced.defines]
                    LSH_COMPACT_ACTUATOR_SWITCH_TIMES = true
                    """,
                ),
            ),
            "LSH_COMPACT_ACTUATOR_SWITCH_TIMES is no longer supported",
        ),
        (
            minimal_profile(
                ProfileParts(device_fields='static_config_include = "../evil.hpp"'),
            ),
            "must be a relative header path",
        ),
        (
            minimal_profile().replace('output_dir = "out"', 'output_dir = "../out"'),
            "generator.output_dir must stay inside the TOML directory",
        ),
        (
            minimal_profile(
                ProfileParts(actuators=DEFAULT_ACTUATOR.replace('"6"', '"6;evil"')),
            ),
            "not allowed in generated C++ expressions",
        ),
        (
            minimal_profile(
                ProfileParts(
                    clickables=DEFAULT_CLICKABLE.replace(
                        'short = "relay"',
                        'long = { targets = ["relay"], after = "66s" }',
                    ),
                ),
            ),
            "between 1 and 65535 ms",
        ),
        (
            minimal_profile(
                ProfileParts(
                    clickables=DEFAULT_CLICKABLE.replace(
                        'short = "relay"',
                        'long = { targets = ["relay"], after = "900ms" }\n'
                        'super_long = { action = "all_off", after = "800ms" }',
                    ),
                ),
            ),
            "must be greater than the long-click threshold",
        ),
        (
            minimal_profile(
                ProfileParts(
                    clickables=DEFAULT_CLICKABLE.replace(
                        'short = "relay"',
                        'super_long = { action = "off" }',
                    ),
                ),
            ),
            "is selective but has no targets",
        ),
        (
            minimal_profile(
                ProfileParts(
                    controller_fields='hardware_include = "<Arduino.h"',
                ),
            ),
            "balanced <header.h> include",
        ),
        (
            minimal_profile(
                ProfileParts(
                    device_fields='build_macro = "LSH_BUILD_SHARED"',
                    extra_sections="""
                    [devices.other]
                    name = "Other"
                    build_macro = "LSH_BUILD_SHARED"

                    [devices.other.actuators.other_relay]
                    id = 2
                    pin = "8"

                    [devices.other.buttons.other_button]
                    id = 2
                    pin = "9"
                    short = "other_relay"
                    """,
                ),
            ),
            "build_macro duplicates",
        ),
        (
            minimal_profile(
                ProfileParts(
                    extra_sections="""
                    [devices.PANEL]
                    name = "Uppercase duplicate"

                    [devices.PANEL.actuators.other_relay]
                    id = 2
                    pin = "8"

                    [devices.PANEL.buttons.other_button]
                    id = 2
                    pin = "9"
                    short = "other_relay"
                    """,
                ),
            ),
            "normalizes to duplicate device key",
        ),
    ],
)
def test_rejects_malformed_profiles_before_generation(
    toml_text: str,
    expected: str,
) -> None:
    """Broken static topologies fail while the user still sees TOML paths."""
    assert_config_error_contains(toml_text, expected)
