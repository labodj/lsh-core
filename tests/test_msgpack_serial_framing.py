"""Host-side regression tests for the MsgPack serial frame receiver."""

from __future__ import annotations

import shutil
import subprocess
import textwrap
from pathlib import Path

import pytest


def test_msgpack_receiver_ignores_noise_and_requires_opening_delimiter(
    tmp_path: Path,
) -> None:
    """Compile the receiver on host and exercise strict frame-start handling."""
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is required for host-side framing regression tests.")

    repo_root = Path(__file__).resolve().parents[1]
    source = tmp_path / "msgpack_framing_test.cpp"
    binary = tmp_path / "msgpack_framing_test"
    source.write_text(
        textwrap.dedent(
            r"""
            #include <cassert>

            #include "communication/msgpack_serial_framing.hpp"

            using lsh::core::transport::MSGPACK_FRAME_END;
            using lsh::core::transport::MSGPACK_FRAME_ESCAPE;
            using lsh::core::transport::MsgPackFrameConsumeResult;
            using lsh::core::transport::MsgPackFrameReceiver;
            using Result = MsgPackFrameConsumeResult;

            int main()
            {
                char buffer[8]{};
                MsgPackFrameReceiver receiver(buffer, sizeof(buffer));
                const auto consume = [&receiver](uint8_t byte, uint32_t now) {
                    return receiver.consumeByte(byte, now);
                };

                assert(consume(0x81U, 1U) == Result::Incomplete);
                assert(receiver.frameLength() == 0U);
                assert(consume(MSGPACK_FRAME_END, 2U) == Result::Incomplete);
                assert(consume(0x01U, 3U) == Result::Incomplete);
                assert(consume(MSGPACK_FRAME_END, 4U) == Result::FrameComplete);
                assert(receiver.frameLength() == 1U);
                assert(receiver.frameData()[0] == 0x01U);

                receiver.reset();
                assert(consume(MSGPACK_FRAME_END, 5U) == Result::Incomplete);
                assert(consume(MSGPACK_FRAME_END, 6U) == Result::Incomplete);
                assert(consume(0x02U, 7U) == Result::Incomplete);
                assert(consume(MSGPACK_FRAME_END, 8U) == Result::FrameComplete);
                assert(receiver.frameLength() == 1U);
                assert(receiver.frameData()[0] == 0x02U);

                receiver.reset();
                assert(consume(MSGPACK_FRAME_END, 9U) == Result::Incomplete);
                assert(consume(MSGPACK_FRAME_ESCAPE, 10U) == Result::Incomplete);
                assert(consume(MSGPACK_FRAME_END, 11U) == Result::FrameDiscarded);
                assert(consume(0x03U, 12U) == Result::Incomplete);
                assert(receiver.frameLength() == 0U);
                assert(consume(MSGPACK_FRAME_END, 13U) == Result::Incomplete);
                assert(consume(0x03U, 14U) == Result::Incomplete);
                assert(consume(MSGPACK_FRAME_END, 15U) == Result::FrameComplete);
                assert(receiver.frameLength() == 1U);
                assert(receiver.frameData()[0] == 0x03U);

                receiver.reset();
                assert(consume(MSGPACK_FRAME_END, 16U) == Result::Incomplete);
                assert(consume(0x04U, 17U) == Result::Incomplete);
                receiver.resetIfIdle(27U, 10U);
                assert(receiver.frameLength() == 0U);
                assert(consume(0x05U, 28U) == Result::Incomplete);
                assert(consume(MSGPACK_FRAME_END, 29U) == Result::Incomplete);
                assert(consume(0x06U, 30U) == Result::Incomplete);
                assert(consume(MSGPACK_FRAME_END, 31U) == Result::FrameComplete);
                assert(receiver.frameLength() == 1U);
                assert(receiver.frameData()[0] == 0x06U);

                char smallBuffer[2]{};
                MsgPackFrameReceiver smallReceiver(smallBuffer, sizeof(smallBuffer));
                const auto consumeSmall = [&smallReceiver](uint8_t byte, uint32_t now) {
                    return smallReceiver.consumeByte(byte, now);
                };
                assert(consumeSmall(MSGPACK_FRAME_END, 32U) == Result::Incomplete);
                assert(consumeSmall(0x07U, 33U) == Result::Incomplete);
                assert(consumeSmall(0x08U, 34U) == Result::Incomplete);
                assert(consumeSmall(0x09U, 35U) == Result::Incomplete);
                assert(smallReceiver.frameLength() == 0U);
                assert(consumeSmall(0x0AU, 36U) == Result::Incomplete);
                assert(consumeSmall(MSGPACK_FRAME_END, 37U) == Result::FrameDiscarded);
                assert(consumeSmall(MSGPACK_FRAME_END, 38U) == Result::Incomplete);
                assert(consumeSmall(0x0BU, 39U) == Result::Incomplete);
                assert(consumeSmall(MSGPACK_FRAME_END, 40U) == Result::FrameComplete);
                assert(smallReceiver.frameLength() == 1U);
                assert(smallReceiver.frameData()[0] == 0x0BU);

                return 0;
            }
            """
        ),
        encoding="utf-8",
    )

    compile_result = subprocess.run(  # noqa: S603 - compiler path comes from shutil.which.
        [
            compiler,
            "-std=c++17",
            "-I",
            str(repo_root / "src"),
            str(source),
            str(repo_root / "src" / "communication" / "msgpack_serial_framing.cpp"),
            "-o",
            str(binary),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    assert compile_result.returncode == 0, compile_result.stderr

    run_result = subprocess.run(  # noqa: S603 - binary is compiled into tmp_path above.
        [str(binary)],
        check=False,
        capture_output=True,
        text=True,
    )
    assert run_result.returncode == 0, run_result.stderr + run_result.stdout
