"""Small checks for ordering invariants in the hand-written runtime loop."""

from pathlib import Path


def test_pulse_timers_advance_before_actions_can_arm_them() -> None:
    """A new pulse must not consume elapsed time from before it was started."""
    source = (Path(__file__).parents[1] / "src" / "core" / "lsh_core.cpp").read_text(
        encoding="utf-8"
    )
    pulse_sweep = source.index("checkPulseTimers(loopElapsed_ms)")
    action_sites = (
        source.index("drainBridgeRx(1U,"),
        source.index("scanClickables(clickableElapsed_ms)"),
        source.index("drainBridgeRx(constants::bridgeSerial::"),
    )

    assert all(pulse_sweep < action_site for action_site in action_sites)
