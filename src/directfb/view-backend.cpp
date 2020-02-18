/*
 * Copyright (C) 2015, 2016 Igalia S.L.
 * Copyright (C) 2015, 2016 Metrological
 * Copyright (C) 2016 SoftAtHome
 * Copyright (C) 1994-2020 OpenTV, Inc. and Nagravision S.A.
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

#ifdef KEY_INPUT_HANDLING_LIBINPUT
#include "Libinput/LibinputServer.h"
#endif

#include "display.h"

#include "ipc.h"
#include "ipc-directfb.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>

namespace Directfb {

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

    Directfb::Display& m_display;

    uint32_t width { 0 };
    uint32_t height { 0 };
};

ViewBackend::ViewBackend(struct wpe_view_backend* backend)
    : backend(backend)
    , m_display(Directfb::Display::singleton())
{
    ipcHost.initialize(*this);
}

ViewBackend::~ViewBackend()
{
    ipcHost.deinitialize();

#ifdef KEY_INPUT_HANDLING_LIBINPUT
    WPE::LibinputServer::singleton().setClient(nullptr);
#endif

}

void ViewBackend::initialize()
{
    fprintf(stdout, "Platform DFB View Backend Initialization\n");

    assert(m_display.width() != 0);
    assert(m_display.height() != 0);

    fprintf(stdout, "DirectFB Display resolution: %dx%d\n", m_display.width(), m_display.height());

    width = m_display.width();
    height = m_display.height();

    wpe_view_backend_dispatch_set_size(backend, width, height);

#ifdef KEY_INPUT_HANDLING_LIBINPUT
    WPE::LibinputServer::singleton().setClient(this);
#endif

}

void ViewBackend::handleFd(int)
{
    fprintf(stderr, "!!!!!!!!!! Not Implemented !!!!!!!!!!\n");
}

void ViewBackend::handleMessage(char* data, size_t size)
{
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
		case IPC::Directfb::BufferCommit::code: {
			auto& bufferCommit = IPC::Directfb::BufferCommit::cast(message);
			commitBuffer(bufferCommit.width, bufferCommit.height);
			break;
		}
		case Directfb::EventDispatcher::MsgType::KEYBOARD: {
			struct wpe_input_keyboard_event * event = reinterpret_cast<wpe_input_keyboard_event*>(std::addressof(message.messageData));
			wpe_view_backend_dispatch_keyboard_event(backend, event);
			break;
		}
		default: {
			fprintf(stderr, "ViewBackend: unhandled message\n");
			break;
		}
	}
}

void ViewBackend::commitBuffer(uint32_t width, uint32_t height)
{
    if (width != this->width || height != this->height)
        return;

    IPC::Message message;
    IPC::Directfb::FrameComplete::construct(message);
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

} // namespace Directfb

extern "C" {

struct wpe_view_backend_interface directfb_view_backend_interface = {
    // create
    [](void*, struct wpe_view_backend* backend) -> void*
    {
        return new Directfb::ViewBackend(backend);
    },
    // destroy
    [](void* data)
    {
        auto* backend = static_cast<Directfb::ViewBackend*>(data);
        delete backend;
    },
    // initialize
    [](void* data)
    {
        auto& backend = *static_cast<Directfb::ViewBackend*>(data);
        backend.initialize();
    },
    // get_renderer_host_fd
    [](void* data) -> int
    {
        auto& backend = *static_cast<Directfb::ViewBackend*>(data);
        return backend.ipcHost.releaseClientFD();
    },
};

}
