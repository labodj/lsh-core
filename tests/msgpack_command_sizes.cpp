// Host regression for the canonical v3 bridge writer and the controller receiver.
#include "communication/msgpack_serial_framing.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <vector>

using namespace lsh::core::transport;
using Bytes = std::vector<uint8_t>;

static void appendUint8(Bytes &payload, uint8_t value)
{
    if (value >= 128U)
    {
        payload.push_back(0xCC);
    }
    payload.push_back(value);
}

template <unsigned Actuators> static void expectAccepted(const Bytes &payload)
{
    std::array<char, maxInboundPayloadSize((Actuators + 7U) / 8U)> buffer{};
    MsgPackFrameReceiver receiver(buffer.data(), buffer.size());
    const auto feed = [&](uint8_t byte) { return receiver.consumeByte(byte, 0U); };
    assert(feed(MSGPACK_FRAME_END) == MsgPackFrameConsumeResult::Incomplete);
    for (uint8_t byte : payload)
    {
        if (byte == MSGPACK_FRAME_END || byte == MSGPACK_FRAME_ESCAPE)
        {
            assert(feed(MSGPACK_FRAME_ESCAPE) == MsgPackFrameConsumeResult::Incomplete);
            byte = byte == MSGPACK_FRAME_END ? MSGPACK_FRAME_ESCAPED_END : MSGPACK_FRAME_ESCAPED_ESCAPE;
        }
        assert(feed(byte) == MsgPackFrameConsumeResult::Incomplete);
    }
    assert(feed(MSGPACK_FRAME_END) == MsgPackFrameConsumeResult::FrameComplete);
    assert(receiver.frameLength() == payload.size());
    assert(std::equal(payload.begin(), payload.end(), receiver.frameData()));
}

template <unsigned Actuators> static void checkSetState()
{
    constexpr unsigned count = (Actuators + 7U) / 8U;
    Bytes payload{0x82, 0xA1, 'p', 12, 0xA1, 's'};
    if (count < 16)
        payload.push_back(0x90U + count);
    else
        payload.insert(payload.end(), {0xDC, 0, count});
    for (unsigned index = 0; index < count; ++index)
    {
        // Set all live bits, leaving unused tail bits zero as required by v3.
        const unsigned liveBits = std::min(8U, Actuators - 8U * index);
        appendUint8(payload, (1U << liveBits) - 1U);
    }
    expectAccepted<Actuators>(payload);
}

int main()
{
    for (uint8_t command : {14, 16})  // NETWORK_CLICK_ACK and FAILOVER_CLICK
        for (uint8_t button : {7, 127, 128, 255})
            for (uint8_t correlation : {127, 128, 192, 219, 255})
            {
                Bytes payload{0x84, 0xA1, 'p', command, 0xA1, 't', 1, 0xA1, 'i'};
                appendUint8(payload, button);
                payload.insert(payload.end(), {0xA1, 'c'});
                appendUint8(payload, correlation);
                expectAccepted<8>(payload);
            }
    static_assert(maxInboundPayloadSize(1) == 15);
    checkSetState<0>();
    checkSetState<32>();
    checkSetState<120>();  // final fixarray
    checkSetState<128>();  // first array16
    checkSetState<255>();  // maximum topology, seven live tail bits

    // Replay the shared UART stream and expose each complete MQTT payload to the host test.
    std::array<char, maxInboundPayloadSize(32)> buffer{};
    MsgPackFrameReceiver receiver(buffer.data(), buffer.size());
    char byte;
    while (std::cin.get(byte))
    {
        const auto result = receiver.consumeByte(static_cast<uint8_t>(byte), 0);
        assert(result != MsgPackFrameConsumeResult::FrameDiscarded);
        if (result == MsgPackFrameConsumeResult::FrameComplete)
        {
            std::cout.put(static_cast<char>(receiver.frameLength()));
            std::cout.write(buffer.data(), receiver.frameLength());
            receiver.reset();
        }
    }
}
