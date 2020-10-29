/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef wpe_platform_ipc_essos_h
#define wpe_platform_ipc_essos_h

#include <memory>
#include <stdint.h>

namespace IPC
{

namespace Essos
{

enum MsgType
{
    AXIS = 0x30,
    POINTER,
    TOUCHSIMPLE,
    KEYBOARD,
    FRAMERENDERED,
    DISPLAYSIZE
};

struct DisplaySize {
    uint32_t width;
    uint32_t height;
    uint8_t padding[24];

    static const uint64_t code = MsgType::DISPLAYSIZE;
    static void construct(Message& message, uint32_t width, uint32_t height)
    {
        message.messageCode = code;

        auto& messageData = *reinterpret_cast<DisplaySize*>(std::addressof(message.messageData));
        messageData.width = width;
        messageData.height = height;
    }
    static DisplaySize& cast(Message& message)
    {
        return *reinterpret_cast<DisplaySize*>(std::addressof(message.messageData));
    }
};
static_assert(sizeof(DisplaySize) == Message::dataSize, "DisplaySize is of correct size");

}  // namespace Essos

}  // namespace IPC

#endif /* wpe_platform_ipc_essos_h */
