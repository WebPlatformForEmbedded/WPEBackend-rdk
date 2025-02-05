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

#include "input.h"
#include "ipc-buffer.h"
#include "ipc.h"
#include <array>
#include <wpe/wpe.h>

namespace Thunder {

constexpr uint16_t DefaultWidth(1280);
constexpr uint16_t DefaultHeight(720);

class ViewBackend : public IPC::Host::Handler {
public:
    ViewBackend() = delete;
    ViewBackend(const ViewBackend&) = delete;
    ViewBackend& operator=(const ViewBackend&) = delete;
    ViewBackend& operator=(ViewBackend&&) = delete;

    ViewBackend(struct wpe_view_backend* backend)
        : _backend(backend)
        , _touchPoints()
        , _ipcHost()
    {
        _ipcHost.initialize(*this);

        _touchPoints.fill({ wpe_input_touch_event_type_null, 0, 0, 0, 0 });
    }

    virtual ~ViewBackend()
    {
        _ipcHost.deinitialize();
    }

    void Initialize()
    {
        uint16_t height = DefaultHeight;
        uint16_t width = DefaultWidth;

        const char* width_text = ::getenv("WEBKIT_RESOLUTION_WIDTH");
        const char* height_text = ::getenv("WEBKIT_RESOLUTION_HEIGHT");

        if (width_text != nullptr) {
            width = ::atoi(width_text);
        }
        if (height_text != nullptr) {
            height = ::atoi(height_text);
        }

        wpe_view_backend_dispatch_set_size(_backend, width, height);
    }

    int ClientFD()
    {
        return _ipcHost.releaseClientFD();
    }

    // IPC::Host::Handler
    void handleFd(int) override { };
    void handleMessage(char* data, size_t size) override
    {
        if (size != IPC::Message::size)
            return;

        auto& message = IPC::Message::cast(data);
        switch (message.messageCode) {
        case Input::MsgType::AXIS: {
            struct wpe_input_axis_event* event = reinterpret_cast<wpe_input_axis_event*>(std::addressof(message.messageData));
            wpe_view_backend_dispatch_axis_event(_backend, event);
            break;
        }
        case Input::MsgType::POINTER: {
            struct wpe_input_pointer_event* event = reinterpret_cast<wpe_input_pointer_event*>(std::addressof(message.messageData));
            wpe_view_backend_dispatch_pointer_event(_backend, event);
            break;
        }
        case Input::MsgType::TOUCH: // UNUSED!
        {
            struct wpe_input_touch_event* event = reinterpret_cast<wpe_input_touch_event*>(std::addressof(message.messageData));
            wpe_view_backend_dispatch_touch_event(_backend, event);
            break;
        }
        case Input::MsgType::TOUCHSIMPLE: {
            struct wpe_input_touch_event_raw* tp = reinterpret_cast<wpe_input_touch_event_raw*>(std::addressof(message.messageData));
            if ((tp->id >= 0) && (tp->id < static_cast<int32_t>(_touchPoints.size()))) {
                auto& point = _touchPoints[tp->id];
                point = { tp->type, tp->time, tp->id, tp->x, tp->y };

                struct wpe_input_touch_event event = { _touchPoints.data(), _touchPoints.size(), tp->type, tp->id, tp->time, 0 };
                wpe_view_backend_dispatch_touch_event(_backend, &event);

                // Free the slot if the touch disappears.
                if (tp->type == wpe_input_touch_event_type_up) {
                    point = { wpe_input_touch_event_type_null, 0, -1, -1, -1 };
                }
            }
            break;
        }
        case Input::MsgType::KEYBOARD: {
            struct wpe_input_keyboard_event* event = reinterpret_cast<wpe_input_keyboard_event*>(std::addressof(message.messageData));
            wpe_view_backend_dispatch_keyboard_event(_backend, event);
            break;
        }
        case IPC::BufferCommit::code: {
            wpe_view_backend_dispatch_frame_displayed(_backend); // Just inform the plugin that we rendered a frame...
            break;
        }
        case IPC::AdjustedDimensions::code: {
            IPC::AdjustedDimensions dimensions = IPC::AdjustedDimensions::cast(message);

            wpe_view_backend_dispatch_set_size(_backend, dimensions.width, dimensions.height);

            fprintf(stdout, "Adjusted (internal buffer) dimensions to %u x %u\n", dimensions.width, dimensions.height);
            break;
        }
        default:
            fprintf(stderr, "ViewBackend: unhandled message\n");
        }
    }

private:
    struct wpe_view_backend* _backend;
    std::array<struct wpe_input_touch_event_raw, 10> _touchPoints;
    IPC::Host _ipcHost;
};

} // namespace Thunder

extern "C" {

struct wpe_view_backend_interface thunder_view_backend_interface = {
    // create
    [](void*, struct wpe_view_backend* backend) -> void* {
        return new Thunder::ViewBackend(backend);
    },
    // destroy
    [](void* data) {
        Thunder::ViewBackend* backend = static_cast<Thunder::ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data) {
        Thunder::ViewBackend& backend(*static_cast<Thunder::ViewBackend*>(data));
        backend.Initialize();
    },
    // get_renderer_host_fd
    [](void* data) -> int {
        Thunder::ViewBackend& backend(*static_cast<Thunder::ViewBackend*>(data));
        return backend.ClientFD();
    },
};
}
