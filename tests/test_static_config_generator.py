"""Regression tests for the TOML-backed static configuration generator."""

from __future__ import annotations

import tempfile
import textwrap
from dataclasses import dataclass
from pathlib import Path

import pytest

from tools import generate_lsh_static_config as gen

DEFAULT_ACTUATOR = """
[[devices.panel.actuators]]
name = "relay"
id = 1
pin = "6"
"""

DEFAULT_CLICKABLE = """
[[devices.panel.clickables]]
name = "button"
id = 1
pin = "7"
short = ["relay"]
"""


@dataclass(frozen=True)
class ProfileParts:
    """Override points for a compact generated TOML fixture."""

    common_fields: str = ""
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
    [generator]
    output_dir = "out"
    config_dir = "generated"
    user_config_header = "lsh_user_config.hpp"

    [common]
    hardware_include = "Arduino.h"
    debug_serial = "Serial"
    com_serial = "Serial1"
    {profile.common_fields}

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
    [[devices.panel.actuators]]
    name = "relay_a"
    id = 1
    pin = "6"
    protected = true

    [[devices.panel.actuators]]
    name = "relay_b"
    id = 9
    pin = "8"
    default_state = true
    auto_off = "10m"
    """
    clickables = """
    [[devices.panel.clickables]]
    name = "button_a"
    id = 2
    pin = "7"
    short = ["relay_a", "relay_b"]
    long = { targets = ["relay_b"], type = "off_only", time = "900ms" }

    [[devices.panel.clickables]]
    name = "button_b"
    id = 9
    pin = "9"
    short = ["relay_a"]
    long = { network = true, fallback = "do_nothing" }
    """
    indicators = """
    [[devices.panel.indicators]]
    name = "led_a"
    pin = "10"
    actuators = ["relay_a", "relay_b"]
    mode = "all"
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
    assert "Actuators::addActuator(&relay_a, 1U, 0U);" in static_header
    assert "DETAILS_JSON_PAYLOAD" in static_header
    assert "DETAILS_MSGPACK_PAYLOAD" in static_header
    assert (
        "auto getLongClickTime(uint8_t clickableIndex) noexcept -> uint16_t"
        in static_header
    )
    assert "return clickableIndex == 0U ? 900U" in static_header
    short_link_signature = (
        "auto getShortClickActuatorLink(uint8_t clickableIndex, "
        "uint8_t linkIndex) noexcept -> uint8_t"
    )
    assert short_link_signature in static_header
    assert "case 0U:" in static_header
    assert "return linkIndex < 2U ? linkIndex : UINT8_MAX;" in static_header
    assert (
        "button_b.setClickableLong(true, true, NoNetworkClickType::DO_NOTHING);"
        in static_header
    )
    assert "if (indicatorIndex == 0U)" in static_header
    assert "return constants::IndicatorMode::ALL;" in static_header


def test_allows_empty_selective_super_long_as_explicit_noop() -> None:
    """Legacy profiles may use empty SELECTIVE super-long to suppress global OFF."""
    clickables = """
    [[devices.panel.clickables]]
    name = "button"
    id = 1
    pin = "7"
    short = ["relay"]
    super_long = { type = "selective" }
    """

    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(
            Path(tmpdir),
            minimal_profile(ProfileParts(clickables=clickables)),
        )
        project = gen.parse_project(config_path)
        files = gen.generated_files(project, ["panel"])

    static_header = next(
        content
        for path, content in files.items()
        if path.name == "panel_static_config.hpp"
    )
    assert "button.setClickableSuperLong(true);" in static_header
    assert (
        "#define LSH_STATIC_CONFIG_SUPER_LONG_CLICK_ACTUATOR_LINKS 0" in static_header
    )


def test_merges_defines_raw_flags_and_environment_names() -> None:
    """Common and device settings resolve exactly as PlatformIO consumes them."""
    extra_sections = """
    [common.defines]
    CONFIG_MSG_PACK = true
    CONFIG_USE_FAST_ACTUATORS = true
    CONFIG_CLICKABLE_DEBOUNCE_TIME_MS = 20
    CONFIG_DISABLED_BY_DEFAULT = false

    [devices.panel.defines]
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
                    common_fields='raw_build_flags = ["-flto"]',
                    device_fields='raw_build_flags = ["-Wl,--gc-sections"]',
                    extra_sections=extra_sections,
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


def test_renders_extreme_static_profiles() -> None:
    """Edge profiles cover zero resources, sparse IDs and every action family."""
    actuators = """
    [[devices.panel.actuators]]
    name = "relay_a"
    id = 1
    pin = "6"
    protected = true

    [[devices.panel.actuators]]
    name = "relay_b"
    id = 17
    pin = "7"
    default_state = true
    auto_off_ms = 255

    [[devices.panel.actuators]]
    name = "relay_c"
    id = 255
    pin = "8"
    auto_off = "65s"
    """
    clickables = """
    [[devices.panel.clickables]]
    name = "button_network"
    id = 250
    pin = "9"
    short = false
    long = { network = true, fallback = "do_nothing", time = "401ms" }
    super_long = { network = true, fallback = "local", time_ms = 1200 }

    [[devices.panel.clickables]]
    name = "button_local"
    id = 2
    pin = "10"
    short = ["relay_a", "relay_c"]
    long = { targets = ["relay_b", "relay_c"], type = "off-only" }
    super_long = { type = "selective" }
    """
    indicators = """
    [[devices.panel.indicators]]
    name = "led_majority"
    pin = "11"
    targets = ["relay_a", "relay_b", "relay_c"]
    mode = "majority"
    """

    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = write_config(
            Path(tmpdir),
            minimal_profile(
                ProfileParts(
                    extra_sections="""
                    [devices.panel.defines]
                    CONFIG_MSG_PACK = true
                    CONFIG_USE_FAST_CLICKABLES = true
                    CONFIG_USE_FAST_ACTUATORS = true
                    CONFIG_USE_FAST_INDICATORS = true
                    CONFIG_ACTUATOR_DEBOUNCE_TIME_MS = 0
                    LSH_COMPACT_ACTUATOR_SWITCH_TIMES = true
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
    assert "button_network.setClickableShort(false);" in static_header
    assert "button_network.setClickableLong" in static_header
    assert "NoNetworkClickType::DO_NOTHING" in static_header
    assert "return constants::IndicatorMode::MAJORITY;" in static_header
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
    assert "custom/maximal_panel_static_config.hpp" in rendered


@pytest.mark.parametrize(
    ("toml_text", "expected"),
    [
        (
            minimal_profile(
                ProfileParts(
                    actuators="""
                    [[devices.panel.actuators]]
                    name = "relay_a"
                    id = 1
                    pin = "6"

                    [[devices.panel.actuators]]
                    name = "relay_b"
                    id = 1
                    pin = "7"
                    """,
                ),
            ),
            "duplicates devices.panel.actuators",
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
                        'short = ["relay"]',
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
                        'short = ["relay"]',
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
                        'short = ["relay"]',
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
                    [devices.panel.defines]
                    LSH_NETWORK_CLICKS = false
                    """,
                    clickables=DEFAULT_CLICKABLE.replace(
                        'short = ["relay"]',
                        "long = { network = true }",
                    ),
                ),
            ),
            "disables network clicks",
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
                        'short = ["relay"]',
                        'long = { targets = ["relay"], time = "66s" }',
                    ),
                ),
            ),
            "between 1 and 65535 ms",
        ),
    ],
)
def test_rejects_malformed_profiles_before_generation(
    toml_text: str,
    expected: str,
) -> None:
    """Broken static topologies fail while the user still sees TOML paths."""
    assert_config_error_contains(toml_text, expected)
