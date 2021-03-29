/*
 * Copyright (C) 2015, 2016 Igalia S.L.
 * Copyright (C) 2015, 2016 Metrological
 * Copyright (C) 2016 SoftAtHome
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef wpe_platform_ipc_wpeframework_h
#define wpe_platform_ipc_wpeframework_h

#include <memory>
#include <stdint.h>

namespace IPC {

struct BufferCommit {
    uint8_t padding[Message::dataSize];

    static const uint64_t code = 1;
    static void construct(Message& message)
    {
        message.messageCode = code;
    }
    static BufferCommit& cast(Message& message)
    {
        return *reinterpret_cast<BufferCommit*>(std::addressof(message.messageData));
    }
};
static_assert(sizeof(BufferCommit) == Message::dataSize, "BufferCommit is of correct size");

struct FrameComplete {
    int8_t padding[Message::dataSize];

    static const uint64_t code = 2;
    static void construct(Message& message)
    {
        message.messageCode = code;
    }
    static FrameComplete& cast(Message& message)
    {
        return *reinterpret_cast<FrameComplete*>(std::addressof(message.messageData));
    }
};
static_assert(sizeof(FrameComplete) == Message::dataSize, "FrameComplete is of correct size");

struct AdjustedDimensions {
    uint32_t width;
    uint32_t height;
    uint8_t padding[24];

    static const uint64_t code = 3;

    static void construct(Message& message, uint32_t width, uint32_t height)
    {
        message.messageCode = code;

        AdjustedDimensions& messageData = *reinterpret_cast<AdjustedDimensions*>(std::addressof(message.messageData));

        messageData.width = width;
        messageData.height = height;
    }

    static AdjustedDimensions& cast(Message& message)
    {
        return *reinterpret_cast<AdjustedDimensions*>(std::addressof(message.messageData));
    }
};
static_assert(sizeof(AdjustedDimensions) == Message::dataSize, "AdjustedDimensions is of incorrect size");

} // namespace IPC

#endif // wpe_platform_ipc_compositor_client_h
