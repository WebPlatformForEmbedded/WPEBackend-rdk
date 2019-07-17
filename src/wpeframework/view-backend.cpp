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

#include <wpe/wpe.h>
#include "display.h"
#include "ipc.h"
#include "ipc-buffer.h"
#include <array>

#define WIDTH 1280
#define HEIGHT 720

namespace WPEFramework {

struct ViewBackend : public IPC::Host::Handler {
    ViewBackend(struct wpe_view_backend*);
    virtual ~ViewBackend();

    // IPC::Host::Handler
    void handleFd(int) override { };
    void handleMessage(char*, size_t) override;

    void ackBufferCommit();
    void initialize();

    static gboolean vsyncCallback(gpointer);

    struct wpe_view_backend* backend;
    std::array<struct wpe_input_touch_event_raw, 10> touchpoints;
    IPC::Host ipcHost;
    GSource* vsyncSource;
    bool triggered;
};

static uint32_t MaxFPS() {
    uint32_t tickDelay = 10; // 100 frames per second should be MAX and the default.
    const char* max_FPS = ::getenv("WEBKIT_MAXIMUM_FPS");

    if (max_FPS != nullptr) {
        uint32_t configValue = ::atoi(max_FPS);
        if ((configValue >= 1) && (configValue <= 100)) {
            tickDelay = (1000 / configValue);
        }
    }
    return (tickDelay);
}
 
ViewBackend::ViewBackend(struct wpe_view_backend* backend)
    : backend(backend)
    , vsyncSource(g_timeout_source_new(MaxFPS()))
    , triggered(false)
{
    ipcHost.initialize(*this);

    g_source_set_callback(vsyncSource, static_cast<GSourceFunc>(vsyncCallback), this, nullptr);
    g_source_set_priority(vsyncSource, G_PRIORITY_HIGH + 30);
    g_source_set_can_recurse(vsyncSource, TRUE);
    g_source_attach(vsyncSource, g_main_context_get_thread_default());

    touchpoints.fill({ wpe_input_touch_event_type_null, 0, 0, 0, 0 });
}

ViewBackend::~ViewBackend()
{
    g_source_destroy(vsyncSource);
    ipcHost.deinitialize();
}

void ViewBackend::handleMessage(char* data, size_t size)
{
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
    case Display::MsgType::AXIS:
    {
        struct wpe_input_axis_event * event = reinterpret_cast<wpe_input_axis_event*>(std::addressof(message.messageData));
        wpe_view_backend_dispatch_axis_event(backend, event);
        break;
    }
    case Display::MsgType::POINTER:
    {
        struct wpe_input_pointer_event * event = reinterpret_cast<wpe_input_pointer_event*>(std::addressof(message.messageData));
        wpe_view_backend_dispatch_pointer_event(backend, event);
        break;
    }
    case Display::MsgType::TOUCH: // UNUSED!
    {
        struct wpe_input_touch_event * event = reinterpret_cast<wpe_input_touch_event*>(std::addressof(message.messageData));
        wpe_view_backend_dispatch_touch_event(backend, event);
        break;
    }
    case Display::MsgType::TOUCHSIMPLE:
    {
        struct wpe_input_touch_event_raw * tp = reinterpret_cast<wpe_input_touch_event_raw*>(std::addressof(message.messageData));
        if ((tp->id >= 0) && (tp->id < static_cast<int32_t>(touchpoints.size()))) {
            auto& point = touchpoints[tp->id];
            point = { tp->type, tp->time, tp->id, tp->x, tp->y };

            struct wpe_input_touch_event event = { touchpoints.data(), touchpoints.size(), tp->type, tp->id, tp->time, 0 };
            wpe_view_backend_dispatch_touch_event(backend, &event);

            // Free the slot if the touch disappears.
            if (tp->type == wpe_input_touch_event_type_up) {
                point = { wpe_input_touch_event_type_null, 0, -1, -1, -1 };
            }
        }
        break;
    }
    case Display::MsgType::KEYBOARD:
    {
        struct wpe_input_keyboard_event * event = reinterpret_cast<wpe_input_keyboard_event*>(std::addressof(message.messageData));
        wpe_view_backend_dispatch_keyboard_event(backend, event);
        break;
    }
    case IPC::BufferCommit::code:
    {
        triggered = true;
        break;
    }
    default:
        fprintf(stderr, "ViewBackend: unhandled message\n");
    }
}

void ViewBackend::initialize()
{
    uint16_t height = HEIGHT;
    uint16_t width = WIDTH;

    const char* width_text = ::getenv("WEBKIT_RESOLUTION_WIDTH");
    const char* height_text = ::getenv("WEBKIT_RESOLUTION_HEIGHT");
    if (width_text != nullptr) {
        width = ::atoi(width_text);
    }
    if (height_text != nullptr) {
        height = ::atoi(height_text);
    }
    wpe_view_backend_dispatch_set_size( backend, width, height);
}

void ViewBackend::ackBufferCommit()
{
    IPC::Message message;
    IPC::FrameComplete::construct(message);
    ipcHost.sendMessage(IPC::Message::data(message), IPC::Message::size);

    wpe_view_backend_dispatch_frame_displayed(backend);
}

gboolean ViewBackend::vsyncCallback(gpointer data)
{
    ViewBackend* impl = static_cast<ViewBackend*>(data);

    if (impl->triggered) {
        impl->triggered = false;
        impl->ackBufferCommit();
    }

    return (G_SOURCE_CONTINUE);
}

} // namespace WPEFramework

extern "C" {

struct wpe_view_backend_interface wpeframework_view_backend_interface = {
    // create
    [](void*, struct wpe_view_backend* backend) -> void*
    {
        return new WPEFramework::ViewBackend(backend);
    },
    // destroy
    [](void* data)
    {
        WPEFramework::ViewBackend* backend = static_cast<WPEFramework::ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data)
    {
        WPEFramework::ViewBackend& backend (*static_cast<WPEFramework::ViewBackend*>(data));
        backend.initialize();
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        WPEFramework::ViewBackend& backend (*static_cast<WPEFramework::ViewBackend*>(data));
        return backend.ipcHost.releaseClientFD();
    },
};

}
