"""Small C++ rendering primitives shared by generated runtime paths."""

from __future__ import annotations

from typing import TYPE_CHECKING

from .constants import CLANG_FORMAT_COLUMN_LIMIT, MAX_INLINE_SUM_TERMS

if TYPE_CHECKING:
    from collections.abc import Sequence


def render_cached_action_time_declaration(indent: str) -> list[str]:
    """Render one generated timestamp shared by a multi-actuator action body."""
    return [
        "#if LSH_CORE_ACTUATOR_NEEDS_SWITCH_TIMESTAMP",
        f"{indent}const uint32_t actionNow = timeKeeper::getTime();",
        "#else",
        f"{indent}constexpr uint32_t actionNow = 0U;",
        "#endif",
    ]


def render_bool_accumulator(
    calls: Sequence[str],
    *,
    variable_name: str = "anyActuatorChangedState",
    indent: str = "        ",
    with_cached_time: bool = False,
) -> list[str]:
    """Render a bool OR accumulator for direct generated actuator calls."""
    if not calls:
        return [f"{indent}return false;"]

    lines: list[str] = []
    if with_cached_time:
        lines.extend(render_cached_action_time_declaration(indent))

    if len(calls) == 1:
        lines.append(f"{indent}return {calls[0]};")
        return lines

    lines.append(f"{indent}bool {variable_name} = false;")
    lines.extend(f"{indent}{variable_name} |= {call};" for call in calls)
    lines.append(f"{indent}return {variable_name};")
    return lines


def render_u8_sum_declaration(
    variable_name: str,
    terms: Sequence[str],
    *,
    indent: str = "        ",
) -> list[str]:
    """Render a clang-format-stable uint8_t sum declaration."""
    if not terms:
        return [f"{indent}const uint8_t {variable_name} = 0U;"]
    joined_terms = " + ".join(terms)
    one_line = f"{indent}const uint8_t {variable_name} = {joined_terms};"
    if (
        len(terms) <= MAX_INLINE_SUM_TERMS
        and len(one_line) <= CLANG_FORMAT_COLUMN_LIMIT
    ):
        return [one_line]

    return [f"{indent}const uint8_t {variable_name} =", f"{indent}    {joined_terms};"]


def render_switch_case_with_body(
    lines: list[str],
    case_index: int,
    body: Sequence[str],
) -> None:
    """Append one uint8_t switch case with a scoped multi-line body."""
    lines.append(f"    case {case_index}U:")
    lines.append("    {")
    lines.extend(body)
    lines.append("    }")
