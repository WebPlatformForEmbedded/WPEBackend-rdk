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

#include "ipc-buffer.h"
#include "ipc.h"

#include "input.h"

#include <chrono>
#include <iostream>
#include <string.h>
#include <string>
#include <thread>

namespace Thunder {
namespace {
    const std::string SuggestedName()
    {
        std::string name = Compositor::IDisplay::SuggestedName();

        if (name.empty()) {
            name = "WebKitBrowser" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        }
        return name;
    }
}

class Display {
public:
    class OffscreenSurface {
        static constexpr uint16_t DefaultHeight = 720;
        static constexpr uint16_t DefaultWidth = 1280;

    public:
        OffscreenSurface(const OffscreenSurface&) = delete;
        OffscreenSurface& operator=(const OffscreenSurface&) = delete;
        OffscreenSurface& operator=(OffscreenSurface&&) = delete;

        OffscreenSurface()
            : _surface(nullptr)
        {
        }

        ~OffscreenSurface()
        {
            if (_surface != nullptr) {
                _surface->Release();
            }
        }

        void Initialize(Display* backend)
        {
            std::string name = "WPEWebkitOffscreenTarget-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            _surface = backend->Create(DefaultWidth, DefaultHeight, nullptr, name);
        }

        EGLNativeWindowType Native() const
        {
            return (_surface != nullptr) ? _surface->Native() : static_cast<EGLNativeWindowType>(0);
        }

    private:
        Compositor::IDisplay::ISurface* _surface;
    };

    class Surface : public IPC::Client::Handler, public Compositor::IDisplay::ISurface::ICallback {
    public:
        Surface() = delete;
        Surface(const Surface&) = delete;
        Surface& operator=(const Surface&) = delete;
        Surface& operator=(Surface&&) = delete;

        Surface(struct wpe_renderer_backend_egl_target* target, int hostFd)
            : _target(target)
            , _ipcClient()
            , _input(_ipcClient)
            , _keyboard(&_input)
            , _wheel(&_input)
            , _pointer(&_input)
            , _touchpanel(&_input)
            , _surface()
            , _triggered(false)
        {
            _ipcClient.initialize(*this, hostFd);
        }

        virtual ~Surface()
        {
            Deinitialize();
            _ipcClient.deinitialize();
        }

        void Initialize(Display* backend, uint32_t width, uint32_t height)
        {
            _surface = backend->Create(width, height, this);

            if (_surface != nullptr) {
                _surface->Keyboard(&_keyboard);
                _surface->Wheel(&_wheel);
                _surface->Pointer(&_pointer);
                _surface->TouchPanel(&_touchpanel);

                if ((width != _surface->Width()) || (height != _surface->Height())) {
                    IPC::Message message;
                    IPC::AdjustedDimensions::construct(message, _surface->Width(), _surface->Height());
                    _ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
                }
            }
        }
        void Deinitialize()
        {
            if (_surface) {
                _surface->Keyboard(nullptr);
                _surface->Wheel(nullptr);
                _surface->Pointer(nullptr);
                _surface->TouchPanel(nullptr);

                _surface->Release();
                _surface = nullptr;
            }
        }

        EGLNativeWindowType Native() const
        {
            return (_surface != nullptr) ? _surface->Native() : static_cast<EGLNativeWindowType>(0);
        }

        void FrameRendered()
        {
            _triggered = true;
            _surface->RequestRender();
        }

        // Compositor::IDisplay::ISurface::ICallback
        void Rendered(Compositor::IDisplay::ISurface*) override
        {
            // Signal that the frame was displayed and can be disposed.
            wpe_renderer_backend_egl_target_dispatch_frame_complete(_target);
        }
        void Published(Compositor::IDisplay::ISurface*) override
        {
            if (_triggered == true) {
                _triggered = false;

                // Inform the plugin to that a frame was displayed, so it can update it's FPS counter
                IPC::Message message;
                IPC::BufferCommit::construct(message);
                _ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
            }
        }

    private:
        // IPC::Client::Handler
        void handleMessage(char* /*data*/, size_t /*size*/) override
        {
            // no messages expected
        }

    private:
        struct wpe_renderer_backend_egl_target* _target;
        IPC::Client _ipcClient;
        Input _input;
        Thunder::KeyboardHandler _keyboard;
        Thunder::WheelHandler _wheel;
        Thunder::PointerHandler _pointer;
        Thunder::TouchPanelHandler _touchpanel;
        Compositor::IDisplay::ISurface* _surface;
        bool _triggered;
    };

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;
    Display& operator=(Display&&) = delete;

    Display()
        : _display(Compositor::IDisplay::Instance(SuggestedName()))
    {
    }

    ~Display()
    {
        _display->Release();
        _display = nullptr;
    }

    EGLNativeDisplayType Native() const
    {
        EGLDisplay display = eglGetDisplay(_display->Native());
        return (_display != nullptr) ? _display->Native() : EGL_NO_DISPLAY;
    }

    uint32_t Platform() const
    {
        return 0;
    }

    Compositor::IDisplay::ISurface* Create(const uint32_t width, const uint32_t height, Compositor::IDisplay::ISurface::ICallback* callback, const std::string& name = SuggestedName())
    {
        return (_display != nullptr) ? _display->Create(name, width, height, callback) : nullptr;
    }

private:
    Compositor::IDisplay* _display;
};
} // namespace Thunder

extern "C" {
struct wpe_renderer_backend_egl_interface thunder_renderer_backend_egl_interface = {
    // create()
    [](int /*ipc_renderer_host_fd*/) -> void* {
        return new Thunder::Display();
    },
    // void destroy
    [](void* data) {
        Thunder::Display* backend(static_cast<Thunder::Display*>(data));
        delete backend;
    },
    // get_native_display
    [](void* data) -> EGLNativeDisplayType {
        Thunder::Display& backend(*static_cast<Thunder::Display*>(data));
        return backend.Native();
    },
#if WPE_CHECK_VERSION(1, 1, 0)
    // get_platform
    [](void* data) -> uint32_t {
        Thunder::Display& backend(*static_cast<Thunder::Display*>(data));
        return backend.Platform();
    },
#endif
};

struct wpe_renderer_backend_egl_target_interface thunder_renderer_backend_egl_target_interface = {
    // create
    [](struct wpe_renderer_backend_egl_target* target, int ipc_webview_host_fd) -> void* {
        return new Thunder::Display::Surface(target, ipc_webview_host_fd);
    },
    // destroy
    [](void* data) {
        Thunder::Display::Surface* target(static_cast<Thunder::Display::Surface*>(data));
        delete target;
    },
    // initialize
    [](void* data, void* egl_backend_data, uint32_t width, uint32_t height) {
        Thunder::Display::Surface& target(*static_cast<Thunder::Display::Surface*>(data));
        target.Initialize(static_cast<Thunder::Display*>(egl_backend_data), width, height);
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType {
        Thunder::Display::Surface& target(*static_cast<Thunder::Display::Surface*>(data));
        return target.Native();
    },
    // resize
    [](void* data, uint32_t width, uint32_t height) {
    },
    // frame_will_render
    [](void* data) {
    },
    // frame_rendered
    [](void* data) {
        Thunder::Display::Surface& target(*static_cast<Thunder::Display::Surface*>(data));
        target.FrameRendered();
    },
#if WPE_CHECK_VERSION(1, 9, 1)
    // deinitialize
    [](void* data) {
        Thunder::Display::Surface& target(*static_cast<Thunder::Display::Surface*>(data));
        target.Deinitialize();
    },
#endif
};

// Note:
// This should work because Render() on the IDisplay::ISurface will never be called. So the compositor will
// not create a texture for this surface, and therefore it won't be able to render it to the screen.
struct wpe_renderer_backend_egl_offscreen_target_interface thunder_renderer_backend_egl_offscreen_target_interface = {
    // create
    []() -> void* {
        return new Thunder::Display::OffscreenSurface();
    },
    // destroy
    [](void* target) {
        Thunder::Display::OffscreenSurface* surface(static_cast<Thunder::Display::OffscreenSurface*>(target));
        delete surface;
    },
    // initialize
    [](void* target, void* backend) {
        Thunder::Display::OffscreenSurface* surface(static_cast<Thunder::Display::OffscreenSurface*>(target));
        surface->Initialize(static_cast<Thunder::Display*>(backend));
    },
    // get_native_window
    [](void* target) -> EGLNativeWindowType {
        Thunder::Display::OffscreenSurface* surface(static_cast<Thunder::Display::OffscreenSurface*>(target));
        return surface->Native();
    },
};
}
