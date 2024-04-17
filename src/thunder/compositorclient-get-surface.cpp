/*
 * Copyright (C) 2020 Linaro
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
#include <wayland-egl-backend.h>
#include "compositorclient-get-surface.h"
#include "display.h"
#include <wpe/wpe-egl.h>
#include <compositor/Client.h>
#include <cstring>

namespace Thunder {

extern "C" {

__attribute__((visibility("default")))
void* wpe_compositorclient_get_parent_surface(struct wpe_renderer_backend_egl* backend)
{
    Compositor::IDisplay::ISurface* surface = nullptr;
    void* nativesurface = nullptr;
    EGLNativeWindowType nativewindow;

    if (!backend)
      return nativesurface;;

    auto* base = reinterpret_cast<struct wpe_renderer_backend_egl_base*>(backend);
    auto* display = reinterpret_cast<Thunder::Compositor::IDisplay*>(base->interface_data);

    //get the ISurface from the IDisplay
    if (display)
        surface = display->SurfaceByName(Compositor::IDisplay::SuggestedName());

    //get the native surface
    if (surface != nullptr) {
	    nativewindow = surface->Native();
#if defined WL_EGL_PLATFORM
	    // On wayland EGLNativeWindowType is struct wl_egl_window * that contains the surface
	    if(nativewindow != nullptr){
            nativesurface  = reinterpret_cast<wl_egl_window*>(nativewindow)->surface;
        }
#endif
    }

    return nativesurface;
}

}

}
