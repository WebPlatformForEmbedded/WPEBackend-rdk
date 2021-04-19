/*
 * Copyright (C) 2017 Metrological
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

#include <wpe/wpe-egl.h>

#include "display.h"
#include "ipc.h"
#include "ipc-buffer.h"

#include <chrono>
#include <string>
#include <string.h>

namespace WPEFramework {

struct EGLTarget : public IPC::Client::Handler {
    EGLTarget(struct wpe_renderer_backend_egl_target*, int);
    virtual ~EGLTarget();

    void initialize(struct wpe_view_backend* backend, uint32_t width, uint32_t height);
    // IPC::Client::Handler
    void handleMessage(char* data, size_t size) override;

    struct wpe_renderer_backend_egl_target* target;
    IPC::Client ipcClient;

    EGLNativeWindowType Native() const {
        return (surface->Native());
    }

    Display display;
    Compositor::IDisplay::ISurface* surface;
};

static std::string DisplayName() {
    std::string name = Compositor::IDisplay::SuggestedName();

    if (name.empty()) {
         name = "WebKitBrowser" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }
    return (name);
}

EGLTarget::EGLTarget(struct wpe_renderer_backend_egl_target* target, int hostFd)
    : target(target)
    , ipcClient()
    , display(ipcClient, DisplayName())
{
    ipcClient.initialize(*this, hostFd);
}

void EGLTarget::initialize(struct wpe_view_backend* backend, uint32_t width, uint32_t height)
{
    surface = display.Create(DisplayName(), width, height);

    using width_t = decltype (width);
    using height_t = decltype (height);

    using s_width_t = decltype (surface->Width());
    using s_height_t = decltype (surface->Width());

    static_assert(std::is_integral<s_width_t>::value, "Integral type required");
    static_assert(std::is_integral<s_height_t>::value, "Integral type required");

    static_assert(sizeof(s_width_t) == sizeof(width_t));
    static_assert(sizeof(s_height_t) == sizeof(height_t));

    s_width_t s_width = surface->Width();
    s_height_t s_height = surface->Height();

    assert(s_width >= 0 && s_height >= 0);

    if (width != static_cast <width_t>(s_width) || height != static_cast<height_t>(s_height)) {
        IPC::Message message;

        IPC::AdjustedDimensions::construct(message, surface->Width(), surface->Height());

        ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
    }

    display.Backend(backend);
}

EGLTarget::~EGLTarget()
{
    ipcClient.deinitialize();
    surface->Release();
}

void EGLTarget::handleMessage(char* data, size_t size)
{
    if (size == IPC::Message::size)
    {
        auto& message = IPC::Message::cast(data);
        switch (message.messageCode) {
        case IPC::FrameComplete::code:
        {
            wpe_renderer_backend_egl_target_dispatch_frame_complete(target);
            break;
        }
        default:
            fprintf(stderr, "EGLTarget: unhandled message\n");
        }
    };
}

} // namespace WPEFramework

extern "C" {

struct wpe_renderer_backend_egl_interface wpeframework_renderer_backend_egl_interface = {
    // create
    [](int input) -> void*
    {
        return WPEFramework::Compositor::IDisplay::Instance(WPEFramework::DisplayName());
    },
    // destroy
    [](void* data)
    {
    },
    // get_native_display
    [](void* data) -> EGLNativeDisplayType
    {
        return WPEFramework::Compositor::IDisplay::Instance(WPEFramework::DisplayName())->Native();
    }
};

struct wpe_renderer_backend_egl_target_interface wpeframework_renderer_backend_egl_target_interface = {
    // create
    [](struct wpe_renderer_backend_egl_target* target, int host_fd) -> void*
    {
        return new WPEFramework::EGLTarget(target, host_fd);
    },
    // destroy
    [](void* data)
    {
        WPEFramework::EGLTarget* target(static_cast<WPEFramework::EGLTarget*>(data));
        delete target;
    },
    // initialize
    [](void* data, void* backend_data, uint32_t width, uint32_t height)
    {
        struct wpe_view_backend* backend (static_cast<struct wpe_view_backend*>(backend_data));
        WPEFramework::EGLTarget& target (*static_cast<WPEFramework::EGLTarget*>(data));
        target.initialize(backend, width, height);
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType
    {
        return static_cast<WPEFramework::EGLTarget*>(data)->Native();
    },
    // resize
    [](void* data, uint32_t width, uint32_t height)
    {
    },
    // frame_will_render
    [](void* data)
    {
    },
    // frame_rendered
    [](void* data)
    {
        WPEFramework::EGLTarget& target (*static_cast<WPEFramework::EGLTarget*>(data));

        /* bool */ target.display.vSyncCallback ();

        // The message has to be sent regardless of the vSyncCallback result.
        // Otherwise, the 'frame complete' is never sent, possibly breaking 
        // the feedback loop.

        IPC::Message message;
        IPC::BufferCommit::construct(message);
        target.ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
    },
};

struct wpe_renderer_backend_egl_offscreen_target_interface wpeframework_renderer_backend_egl_offscreen_target_interface = {
    // create
    []() -> void*
    {
        return nullptr;
    },
    // destroy
    [](void* data)
    {
    },
    // initialize
    [](void* data, void* backend_data)
    {
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType
    {
        return (EGLNativeWindowType) 0;
    },
};

}
