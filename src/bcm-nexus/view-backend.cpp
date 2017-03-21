/*
 * Copyright (C) 2015, 2016 Igalia S.L.
 * Copyright (C) 2015, 2016 Metrological
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

#include <wpe/view-backend.h>

#ifdef KEY_INPUT_HANDLING_LIBINPUT
#include "Libinput/LibinputServer.h"
#endif

#ifdef KEY_INPUT_HANDLING_WAYLAND
#include "display.h"
#include <wayland-client.h>
#endif

#include "ipc.h"
#include "ipc-bcmnexus.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>

namespace BCMNexus {

using FormatTuple = std::tuple<const char*, uint32_t, uint32_t>;
static const std::array<FormatTuple, 9> s_formatTable = {
   FormatTuple{ "1080i", 1920, 1080 },
   FormatTuple{ "720p", 1280, 720 },
   FormatTuple{ "480p", 640, 480 },
   FormatTuple{ "480i", 640, 480 },
   FormatTuple{ "720p50Hz", 1280, 720 },
   FormatTuple{ "1080p24Hz", 1920, 1080 },
   FormatTuple{ "1080i50Hz", 1920, 1080 },
   FormatTuple{ "1080p50Hz", 1920, 1080 },
   FormatTuple{ "1080p60Hz", 1920, 1080 },
};

#ifdef KEY_INPUT_HANDLING_WAYLAND
static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
                                                       uint32_t serial)
{
       wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
                uint32_t edges, int32_t width, int32_t height)
{
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
       handle_ping,
       handle_configure,
       handle_popup_done
};
#endif

struct ViewBackend : public IPC::Host::Handler
#ifdef KEY_INPUT_HANDLING_LIBINPUT
                   , public WPE::LibinputServer::Client
#endif
{
    ViewBackend(struct wpe_view_backend*);
    virtual ~ViewBackend();

    void initialize();

    // IPC::Host::Handler
    void handleFd(int) override;
    void handleMessage(char*, size_t) override;

    void commitBuffer(uint32_t, uint32_t);

#ifdef KEY_INPUT_HANDLING_LIBINPUT
    // WPE::LibinputServer::Client
    void handleKeyboardEvent(struct wpe_input_keyboard_event*) override;
    void handlePointerEvent(struct wpe_input_pointer_event*) override;
    void handleAxisEvent(struct wpe_input_axis_event*) override;
    void handleTouchEvent(struct wpe_input_touch_event*) override;
#endif

    struct wpe_view_backend* backend;
    IPC::Host ipcHost;

#ifdef KEY_INPUT_HANDLING_WAYLAND
    Wayland::Display& m_display;
    struct wl_surface* m_surface { nullptr };
    struct wl_shell_surface *m_shellSurface { nullptr };
#endif

    uint32_t width { 0 };
    uint32_t height { 0 };
};

ViewBackend::ViewBackend(struct wpe_view_backend* backend)
    : backend(backend)
#ifdef KEY_INPUT_HANDLING_WAYLAND
    , m_display(Wayland::Display::singleton())
#endif
{
    ipcHost.initialize(*this);
}

ViewBackend::~ViewBackend()
{
    ipcHost.deinitialize();

#ifdef KEY_INPUT_HANDLING_LIBINPUT
    WPE::LibinputServer::singleton().setClient(nullptr);
#endif

#ifdef KEY_INPUT_HANDLING_WAYLAND
    m_display.unregisterInputClient(m_surface);
    if (m_shellSurface)
        wl_shell_surface_destroy(m_shellSurface);
    m_shellSurface = nullptr;
    if (m_surface)
        wl_surface_destroy(m_surface);
    m_surface = nullptr;
#endif
}

void ViewBackend::initialize()
{
    const char* format = std::getenv("WPE_NEXUS_FORMAT");
    if (!format)
        format = "720p";
    auto it = std::find_if(s_formatTable.cbegin(), s_formatTable.cend(),
        [format](const FormatTuple& t) { return !std::strcmp(std::get<0>(t), format); });
    assert(it != s_formatTable.end());
    auto& selectedFormat = *it;

    width = std::get<1>(selectedFormat);
    height = std::get<2>(selectedFormat);
    fprintf(stderr, "ViewBackend: selected format '%s' (%d,%d)\n",
        std::get<0>(selectedFormat), width, height);

    wpe_view_backend_dispatch_set_size(backend, width, height);

#ifdef KEY_INPUT_HANDLING_LIBINPUT
    WPE::LibinputServer::singleton().setClient(this);
#endif

#ifdef KEY_INPUT_HANDLING_WAYLAND
    m_surface = wl_compositor_create_surface(m_display.interfaces().compositor);
    if (m_display.interfaces().shell) {
        m_shellSurface = wl_shell_get_shell_surface(m_display.interfaces().shell, m_surface);
        if (m_shellSurface) {
            wl_shell_surface_add_listener(m_shellSurface,
                                          &shell_surface_listener, NULL);
        }
    }

    m_display.registerInputClient(m_surface, backend);
#endif
}

void ViewBackend::handleFd(int)
{
}

void ViewBackend::handleMessage(char* data, size_t size)
{
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
    case IPC::BCMNexus::BufferCommit::code:
    {
        auto& bufferCommit = IPC::BCMNexus::BufferCommit::cast(message);
        commitBuffer(bufferCommit.width, bufferCommit.height);
        break;
    }
    default:
        fprintf(stderr, "ViewBackend: unhandled message\n");
    }
}

void ViewBackend::commitBuffer(uint32_t width, uint32_t height)
{
    if (width != this->width || height != this->height)
        return;

    IPC::Message message;
    IPC::BCMNexus::FrameComplete::construct(message);
    ipcHost.sendMessage(IPC::Message::data(message), IPC::Message::size);

    wpe_view_backend_dispatch_frame_displayed(backend);
}

#ifdef KEY_INPUT_HANDLING_LIBINPUT
void ViewBackend::handleKeyboardEvent(struct wpe_input_keyboard_event* event)
{
    wpe_view_backend_dispatch_keyboard_event(backend, event);
}

void ViewBackend::handlePointerEvent(struct wpe_input_pointer_event* event)
{
    wpe_view_backend_dispatch_pointer_event(backend, event);
}

void ViewBackend::handleAxisEvent(struct wpe_input_axis_event* event)
{
    wpe_view_backend_dispatch_axis_event(backend, event);
}

void ViewBackend::handleTouchEvent(struct wpe_input_touch_event* event)
{
    wpe_view_backend_dispatch_touch_event(backend, event);
}
#endif

} // namespace BCMNexus

extern "C" {

struct wpe_view_backend_interface bcm_nexus_view_backend_interface = {
    // create
    [](void*, struct wpe_view_backend* backend) -> void*
    {
        return new BCMNexus::ViewBackend(backend);
    },
    // destroy
    [](void* data)
    {
        auto* backend = static_cast<BCMNexus::ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data)
    {
        auto& backend = *static_cast<BCMNexus::ViewBackend*>(data);
        backend.initialize();
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        auto& backend = *static_cast<BCMNexus::ViewBackend*>(data);
        return backend.ipcHost.releaseClientFD();
    },
};

}
