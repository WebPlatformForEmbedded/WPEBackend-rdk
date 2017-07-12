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

#include <wayland-egl.h>
#include <wpe/renderer-backend-egl.h>

#include <Client.h>
#include <pthread.h>

#include "ipc.h"
#include "ipc-waylandegl.h"

namespace CompositorClient {

class Process : public WPEFramework::Wayland::Display::IProcess {
 public:
  virtual bool Dispatch()
  {
      return true;
  };
};

static Process process;

static void* Processor(void* arg)
{
    printf("[%s:%d] Spin-up the wayland hamster.\n", __FILE__, __LINE__);
    WPEFramework::Wayland::Display::Instance().Process(SIGINT, &process);
    printf("[%s:%d] The wayland hamster left the wheel.\n", __FILE__, __LINE__);
}

static bool inputHandler(const uint32_t state, const uint32_t code, const uint32_t modifiers)
{
    printf("[Wayland Input] Received input,  0X%02X was %s\n", code, state == 0 ? "released" : "pressed");

    switch (state) {
        case WL_KEYBOARD_KEY_STATE_PRESSED:
            printf("[Wayland Input] Sending key pressed [%d] to WPE.\n", code);
            break;
        case WL_KEYBOARD_KEY_STATE_RELEASED:
            printf("[Wayland Input] Sending key released [%d] to WPE.\n", code);
            break;
        default:;
    }

    return true;
}

struct Backend {
    Backend();
    ~Backend();

    WPEFramework::Wayland::Display& display;
    WPEFramework::Wayland::Display::Surface* client;
};

Backend::Backend()
    : display(WPEFramework::Wayland::Display::Instance())
    , client(nullptr)
{
    pthread_t tid;
    const char* envName = ::getenv("COMPOSITOR_CLIENT_NAME");

    assert(envName == nullptr);

    if (envName != nullptr) {
        client = new WPEFramework::Wayland::Display::Surface(display.Create(envName, 1280, 720));
    }

    // set input calback
    display.Callback(inputHandler);

    // create a wayland loop
    if (pthread_create(&tid, NULL, Processor, NULL) != 0) {
        printf("[Wayland] Error creating processor thread\n");
    }


}

Backend::~Backend()
{
    if (display.IsOperational()) {
        // exit wayland loop
        display.Signal();
    }

    if (client != nullptr) {
        delete client;
    }
}

struct EGLTarget : public IPC::Client::Handler {
    EGLTarget(struct wpe_renderer_backend_egl_target*, int);
    virtual ~EGLTarget();

    void initialize(Backend& backend, uint32_t width, uint32_t height);
    // IPC::Client::Handler
    void handleMessage(char* data, size_t size) override;

    struct wpe_renderer_backend_egl_target* target;
    IPC::Client ipcClient;

    Backend* m_backend{ nullptr };
};

EGLTarget::EGLTarget(struct wpe_renderer_backend_egl_target* target, int hostFd)
    : target(target)
{
    ipcClient.initialize(*this, hostFd);
   //  Wayland::EventDispatcher::singleton().setIPC(ipcClient);
}

void EGLTarget::initialize(Backend& backend, uint32_t width, uint32_t height)
{
    m_backend = &backend;
}

EGLTarget::~EGLTarget()
{
    ipcClient.deinitialize();
}

void EGLTarget::handleMessage(char* data, size_t size)
{
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
    case IPC::CompositorClient::FrameComplete::code: {
        wpe_renderer_backend_egl_target_dispatch_frame_complete(target);
        break;
    }
    default:
        fprintf(stderr, "EGLTarget: unhandled message\n");
    };
}

} // namespace CompositorClient

extern "C" {

struct wpe_renderer_backend_egl_interface compositor_client_renderer_backend_egl_interface = {
    // create
    [](int) -> void* {
        return new CompositorClient::Backend;
    },
    // destroy
    [](void* data) {
        auto* backend = static_cast<CompositorClient::Backend*>(data);
        delete backend;
    },
    // get_native_display
    [](void* data) -> EGLNativeDisplayType {
        auto& backend = *static_cast<CompositorClient::Backend*>(data);
    },
};

struct wpe_renderer_backend_egl_target_interface compositor_client_renderer_backend_egl_target_interface = {
    // create
    [](struct wpe_renderer_backend_egl_target* target, int host_fd) -> void* {
        return new CompositorClient::EGLTarget(target, host_fd);
    },
    // destroy
    [](void* data) {
        auto* target = static_cast<CompositorClient::EGLTarget*>(data);
        delete target;
    },
    // initialize
    [](void* data, void* backend_data, uint32_t width, uint32_t height) {
        auto& target = *static_cast<CompositorClient::EGLTarget*>(data);
        auto& backend = *static_cast<CompositorClient::Backend*>(backend_data);
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType {
        auto& target = *static_cast<CompositorClient::EGLTarget*>(data);
    },
    // resize
    [](void* data, uint32_t width, uint32_t height) {
    },
    // frame_will_render
    [](void* data) {
    },
    // frame_rendered
    [](void* data) {
        auto& target = *static_cast<CompositorClient::EGLTarget*>(data);


        IPC::Message message;
        IPC::CompositorClient::BufferCommit::construct(message);
        target.ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
    },
};

struct wpe_renderer_backend_egl_offscreen_target_interface compositor_client_renderer_backend_egl_offscreen_target_interface = {
    // create
    []() -> void* {
        return nullptr;
    },
    // destroy
    [](void* data) {
    },
    // initialize
    [](void* data, void* backend_data) {
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType {
        return nullptr;
    },
};
}
