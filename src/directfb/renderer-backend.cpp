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

#include <wpe/wpe-egl.h>
#include <EGL/egl.h>

#include "display.h"
#include "ipc.h"
#include "ipc-directfb.h"
#include <cstring>
#include <stdio.h>
#include <pthread.h>
#include <cassert>

#define OPENGL_ES_2 1

#include <directfb.h>

namespace Directfb {

static IDirectFB            *s_dfb = 0;

struct Backend {
    Backend();
    ~Backend();

    Directfb::Display&  display;
};

Backend::Backend()
    : display(Directfb::Display::singleton())
{
    fprintf(stdout, "Platform DFB Backend Initialization\n");
}

Backend::~Backend()
{
    fprintf(stderr, "!!!!!!!!!! Not Implemented !!!!!!!!!!\n");
}

struct EGLTarget : public IPC::Client::Handler {
    EGLTarget(struct wpe_renderer_backend_egl_target*, int);
    virtual ~EGLTarget();

    void initialize(Backend& backend, uint32_t, uint32_t);
    void resize(uint32_t, uint32_t);

    // IPC::Client::Handler
    void handleMessage(char*, size_t) override;

    struct wpe_renderer_backend_egl_target* target;
    IPC::Client ipcClient;
    void* nativeWindow;
    void* dfbWindow;
    bool windowSuspended;
    DFBSurfaceCapabilities surfaceCaps;
    Backend* m_backend { nullptr };
    uint32_t width { 0 };
    uint32_t height { 0 };
};

EGLTarget::EGLTarget(struct wpe_renderer_backend_egl_target* target, int hostFd)
    : target(target)
    , nativeWindow(nullptr)
    , dfbWindow(nullptr)
    , windowSuspended(false)
    , surfaceCaps(DSCAPS_NONE)
{
    ipcClient.initialize(*this, hostFd);
    Directfb::EventDispatcher::singleton().setIPC( ipcClient );
}

EGLTarget::~EGLTarget()
{
    ipcClient.deinitialize();
    fprintf(stderr, "!!!!!!!!!! Not Implemented !!!!!!!!!!\n");
}

static void
update_surface_capabilities(IDirectFBWindow *window ,DFBWindowDescription *Desc)
{
    /* For WPE_DFB_WINDOW_FLAGS setenv is in mainc.pp  */
    const char * window_flags_str = getenv("WPE_DFB_WINDOW_FLAGS");
    fprintf(stdout, "%s (window-flags for RDK = %s )\n", __FUNCTION__, window_flags_str );

    DFBSurfaceCapabilities dscaps_premultiplied = DSCAPS_NONE;
    DFBSurfaceCapabilities dscaps_buffer = DSCAPS_NONE;
    DFBSurfaceCapabilities dscaps_gl = DSCAPS_NONE;

    if(window_flags_str) {
        gchar** window_flags = g_strsplit(window_flags_str, ",", -1);
        if(window_flags) {
            unsigned int i = 0;
            while(window_flags[i]) {
                if(strcmp("premultiplied", window_flags[i]) == 0)
                    dscaps_premultiplied = DSCAPS_PREMULTIPLIED;
                else if (strcmp("double-buffer", window_flags[i]) == 0)
                    dscaps_buffer = DSCAPS_DOUBLE;
                else if (strcmp("triple-buffer", window_flags[i]) == 0)
                    dscaps_buffer = DSCAPS_TRIPLE;
                else if (strcmp("opengl", window_flags[i]) == 0)
                    dscaps_gl = DSCAPS_GL;
                i++;
            }
            Desc->surface_caps = DFBSurfaceCapabilities(dscaps_gl | dscaps_premultiplied | dscaps_buffer);
            g_strfreev(window_flags);
        }
    }
}

void EGLTarget::resize(uint32_t w, uint32_t h)
{
    assert(dfbWindow);
    IDirectFBWindow *dfb_window = reinterpret_cast<IDirectFBWindow*>(dfbWindow);

    if(w <= 1 && h <= 1) {
        // if width and height are 1 or less, the app is being made invisible, so we can free graphics memory by resizing the surface to a minimum
        fprintf(stdout, "WPE Backend resize(): suspending DFB Window\n");

        dfb_window->ResizeSurface(dfb_window, w, h);
        windowSuspended = true;
    }else if(windowSuspended) {
        fprintf(stdout, "WPE Backend resize(): resuming DFB Window\n");

        // if window was suspended previously and now we are restoring it by setting width and height to larger than 1, we need to resize the surface back to original size
        dfb_window->ResizeSurface(dfb_window, this->width, this->height);
        windowSuspended = false;
    }

    assert(nativeWindow);
    IDirectFBSurface *dfb_surface = reinterpret_cast<IDirectFBSurface*>(nativeWindow);
    dfb_surface->Clear(dfb_surface, 0x0, 0x0, 0x0, 0x0);
    dfb_surface->Flip(dfb_surface, NULL, DSFLIP_WAITFORSYNC);

    if (surfaceCaps & DSCAPS_DOUBLE){
        // one more time since it's a double surface
        dfb_surface->Clear(dfb_surface, 0x0, 0x0, 0x0, 0x0);
        dfb_surface->Flip(dfb_surface, NULL, DSFLIP_WAITFORSYNC);
    }else if(surfaceCaps & DSCAPS_TRIPLE){
        // two more times since it's a triple surface
        dfb_surface->Clear(dfb_surface, 0x0, 0x0, 0x0, 0x0);
        dfb_surface->Flip(dfb_surface, NULL, DSFLIP_WAITFORSYNC);
        dfb_surface->Clear(dfb_surface, 0x0, 0x0, 0x0, 0x0);
        dfb_surface->Flip(dfb_surface, NULL, DSFLIP_WAITFORSYNC);
    }
}

void EGLTarget::initialize(Backend& backend, uint32_t width, uint32_t height)
{
    DFBResult              res;
    DFBWindowDescription   desc;

    IDirectFBSurface *dfb_surface = NULL;
    IDirectFBDisplayLayer *layer = NULL;
    IDirectFBWindow *dfb_window = NULL;
    DFBDisplayLayerConfig layerConfig;

    fprintf(stdout, "!!!!!!!!!! EGLTarget::initialize Creating DFB Window !!!!!!!!!!\n");

    m_backend = &backend;

    if (nativeWindow)
        return;

    s_dfb = m_backend->display.iDirectfb();
    s_dfb->GetDisplayLayer( s_dfb, DLID_PRIMARY, &layer );
    layer->SetCooperativeLevel( layer, DLSCL_SHARED );
    layer->GetConfiguration(layer, &layerConfig);

    fprintf(stdout, "Requested WidthxHeight = %dx%d, Layer Configuration %dx%d\n", width, height, layerConfig.width, layerConfig.height);

    desc.posx   = 0;
    desc.posy   = 0;
    desc.width  = width;
    desc.height = height;
    desc.pixelformat = DSPF_ABGR;
    desc.surface_caps = DFBSurfaceCapabilities(DSCAPS_DOUBLE | DSCAPS_PREMULTIPLIED);
    desc.flags  = DFBWindowDescriptionFlags(DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_PIXELFORMAT | DWDESC_CAPS | DWDESC_SURFACE_CAPS);
    desc.caps = DFBWindowCapabilities(DWCAPS_ALPHACHANNEL | DWCAPS_DOUBLEBUFFER);

    update_surface_capabilities(dfb_window, &desc);
    res = layer->CreateWindow( layer, &desc, &dfb_window );

    // clear window contents
    dfb_window->GetSurface( dfb_window, &dfb_surface );
    dfb_surface->Clear( dfb_surface, 0,0,0,0);
    dfb_surface->Flip( dfb_surface, NULL, DSFLIP_NONE );
    dfb_surface->Clear( dfb_surface, 0,0,0,0);

    // make it visible
    dfb_window->SetOpacity( dfb_window, 0xff);
    dfb_window->RaiseToTop( dfb_window );

    layer->Release( layer );
    nativeWindow = (void*)dfb_surface;
    dfbWindow = (void*)dfb_window;
    surfaceCaps = desc.surface_caps;

    this->width = width;
    this->height = height;

    m_backend->display.initializeEventSource(dfb_window);
}

void EGLTarget::handleMessage(char* data, size_t size)
{
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
        case IPC::Directfb::FrameComplete::code: {
            wpe_renderer_backend_egl_target_dispatch_frame_complete(target);
            break;
        }
        default:
            fprintf(stderr, "EGLTarget: unhandled message\n");
    };
}

} // namespace Directfb

extern "C" {

struct wpe_renderer_backend_egl_interface directfb_renderer_backend_egl_interface = {
    // create
    [](int) -> void*
    {
        return new Directfb::Backend;
    },
    // destroy
    [](void* data)
    {
        auto* backend = static_cast<Directfb::Backend*>(data);
        delete backend;
    },
    // get_native_display
    [](void* data) -> EGLNativeDisplayType
    {
        auto& backend = *static_cast<Directfb::Backend*>(data);
        backend.display.registerDirectFBDisplayPlatform();
        return EGL_DEFAULT_DISPLAY;
    },
};

struct wpe_renderer_backend_egl_target_interface directfb_renderer_backend_egl_target_interface = {
    // create
    [](struct wpe_renderer_backend_egl_target* target, int host_fd) -> void*
    {
        return new Directfb::EGLTarget(target, host_fd);
    },
    // destroy
    [](void* data)
    {
        auto* target = static_cast<Directfb::EGLTarget*>(data);
        delete target;
    },
    // initialize
    [](void* data, void* backend_data, uint32_t width, uint32_t height)
    {
        auto& target = *static_cast<Directfb::EGLTarget*>(data);
        auto& backend = *static_cast<Directfb::Backend*>(backend_data);
        target.initialize(backend, width, height);
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType
    {
        auto& target = *static_cast<Directfb::EGLTarget*>(data);
        return (EGLNativeWindowType)target.nativeWindow;
    },
    // resize
    [](void* data, uint32_t width, uint32_t height)
    {
        auto& target = *static_cast<Directfb::EGLTarget*>(data);
        target.resize(width, height);
    },
    // frame_will_render
    [](void* data)
    {
    },
    // frame_rendered
    [](void* data)
    {
        auto& target = *static_cast<Directfb::EGLTarget*>(data);

        IPC::Message message;
        IPC::Directfb::BufferCommit::construct(message, target.width, target.height);
        target.ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
    },
};

struct wpe_renderer_backend_egl_offscreen_target_interface directfb_renderer_backend_egl_offscreen_target_interface = {
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
        return (EGLNativeWindowType)nullptr;
    },
};

}
