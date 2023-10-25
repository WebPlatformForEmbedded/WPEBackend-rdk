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

#include <wpe/wpe.h>

#include <string.h>
#include <stdlib.h>
#include <cstdio>

#include "ipc.h"
#include "ipc-essos.h"

#if !defined(DEFAULT_WIDTH)
#define DEFAULT_WIDTH (1280)
#endif

#if !defined(DEFAULT_HEIGHT)
#define DEFAULT_HEIGHT (720)
#endif

#define ERROR_LOG(fmt, ...) fprintf(stderr, "[essos:view-backend.cpp:%u:%s] *** " fmt "\n",  __LINE__, __func__, ##__VA_ARGS__)

namespace Essos {

struct ViewBackend : public IPC::Host::Handler
{
    ViewBackend(struct wpe_view_backend*);
    virtual ~ViewBackend();

    // IPC::Host::Handler
    void handleFd(int) override { };
    void handleMessage(char*, size_t) override;

    void initialize();

    struct wpe_view_backend* backend;
    IPC::Host ipcHost;
};

ViewBackend::ViewBackend(struct wpe_view_backend* backend)
    : backend(backend)
{
    const char* identifier = getenv("CLIENT_IDENTIFIER");
    if (identifier)
    {
        const char* tmp = strchr(identifier, ',');
        if (tmp)
        {
            const char* targetDisplay = tmp + 1;
            setenv("WAYLAND_DISPLAY", targetDisplay, 1);
        }
    }
    ipcHost.initialize(*this);
}

ViewBackend::~ViewBackend()
{
    ipcHost.deinitialize();
}

void ViewBackend::handleMessage(char* data, size_t size)
{
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
    case IPC::Essos::MsgType::AXIS:
    {
        struct wpe_input_axis_event * event = reinterpret_cast<wpe_input_axis_event*>(std::addressof(message.messageData));
        wpe_view_backend_dispatch_axis_event(backend, event);
        break;
    }
    case IPC::Essos::MsgType::POINTER:
    {
        struct wpe_input_pointer_event * event = reinterpret_cast<wpe_input_pointer_event*>(std::addressof(message.messageData));
        wpe_view_backend_dispatch_pointer_event(backend, event);
        break;
    }
    case IPC::Essos::MsgType::TOUCHSIMPLE:
    {
        struct wpe_input_touch_event_raw * touchpoint = reinterpret_cast<wpe_input_touch_event_raw*>(std::addressof(message.messageData));
        struct wpe_input_touch_event event = { touchpoint, 1, touchpoint->type, touchpoint->id, touchpoint->time, 0 };
        wpe_view_backend_dispatch_touch_event(backend, &event);
        break;
    }
    case IPC::Essos::MsgType::KEYBOARD:
    {
        struct wpe_input_keyboard_event * event = reinterpret_cast<wpe_input_keyboard_event*>(std::addressof(message.messageData));
        wpe_view_backend_dispatch_keyboard_event(backend, event);
        break;
    }
    case IPC::Essos::MsgType::FRAMERENDERED:
    {
        wpe_view_backend_dispatch_frame_displayed(backend);
        break;
    }
    case IPC::Essos::MsgType::DISPLAYSIZE:
    {
        auto& displaySize = IPC::Essos::DisplaySize::cast(message);
        wpe_view_backend_dispatch_set_size(backend, displaySize.width, displaySize.height);
        break;
    }
    default:
        ERROR_LOG("ViewBackend: unhandled message (%d)", message.messageCode);
        break;
    }
}

void ViewBackend::initialize()
{
    int32_t width = DEFAULT_WIDTH;
    int32_t height = DEFAULT_HEIGHT;

    if (auto *w = getenv("WEBKIT_RESOLUTION_WIDTH")) {
        int32_t tmp = atoi(w);
        if (tmp > 0)
            width = tmp;
    }

    if (auto *h = getenv("WEBKIT_RESOLUTION_HEIGHT")) {
        int32_t tmp = atoi(h);
        if (tmp > 0)
            height = tmp;
    }

    wpe_view_backend_dispatch_set_size(backend, width, height);
}

} // namespace Essos

extern "C" {

struct wpe_view_backend_interface essos_view_backend_interface = {
    // create
    [](void*, struct wpe_view_backend* backend) -> void*
    {
        return new Essos::ViewBackend(backend);
    },
    // destroy
    [](void* data)
    {
        auto* backend = static_cast<Essos::ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data)
    {
        auto& backend = *static_cast<Essos::ViewBackend*>(data);
        backend.initialize();
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        auto& backend = *static_cast<Essos::ViewBackend*>(data);
        return backend.ipcHost.releaseClientFD();
    },
};

}
