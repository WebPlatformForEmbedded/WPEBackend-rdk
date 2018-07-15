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

#include <wpe/wpe-egl.h>

#include "ipc.h"
#include "ipc-bcmnexus.h"
#include <EGL/egl.h>
#include <cstring>
#include <stdio.h>
#include <refsw/nexus_config.h>
#include <refsw/nexus_platform.h>
#include <refsw/nexus_display.h>
#include <refsw/nexus_core_utils.h>
#include <refsw/default_nexus.h>

#ifdef BACKEND_BCM_NEXUS_NXCLIENT
#include <refsw/nxclient.h>
#endif

#include <chrono>
#include <string>

class Client {
public:
    Client()
            : _name("WebKitBrowser_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()))
            , _id(~0)
    {
        char *tmp;

        if (tmp = getenv("CLIENT_IDENTIFIER")) {
            char *token = strtok(tmp, ",");

            if (token != NULL)
            {
                _id = atoi(token);
            }

            token = strtok(NULL, ",");

            if (token != NULL)
            {
                _name =  std::string(token);
            }
        }
    }

public:
    std::string Name(){
        return _name;
    }

    uint8_t ID(){
        return _id;
    }

private:
    std::string _name;
    uint8_t _id;
};

static Client g_client;

namespace BCMNexus {

struct Backend {
    Backend();
    ~Backend();

    NXPL_PlatformHandle nxplHandle;
};

Backend::Backend()
{
    NEXUS_DisplayHandle displayHandle(nullptr);

#ifdef BACKEND_BCM_NEXUS_NXCLIENT
    NxClient_JoinSettings joinSettings;
    NxClient_GetDefaultJoinSettings(&joinSettings);

    strcpy(joinSettings.name, g_client.Name().c_str());

    NEXUS_Error rc = NxClient_Join(&joinSettings);
    BDBG_ASSERT(!rc);
#else
    NEXUS_Error rc = NEXUS_Platform_Join();
    BDBG_ASSERT(!rc);
#endif

    NXPL_RegisterNexusDisplayPlatform(&nxplHandle, displayHandle);
}

Backend::~Backend()
{
    NXPL_UnregisterNexusDisplayPlatform(nxplHandle);
#ifdef BACKEND_BCM_NEXUS_NXCLIENT
    NxClient_Uninit();
#else
   NEXUS_Platform_Uninit();
#endif
}

struct EGLTarget : public IPC::Client::Handler {
    EGLTarget(struct wpe_renderer_backend_egl_target*, int);
    virtual ~EGLTarget();

    void initialize(uint32_t, uint32_t);

    // IPC::Client::Handler
    void handleMessage(char*, size_t) override;

    struct wpe_renderer_backend_egl_target* target;
    IPC::Client ipcClient;

    void* nativeWindow;
    uint32_t width { 0 };
    uint32_t height { 0 };
};

EGLTarget::EGLTarget(struct wpe_renderer_backend_egl_target* target, int hostFd)
    : target(target)
    , nativeWindow(nullptr)
{
    ipcClient.initialize(*this, hostFd);
}

EGLTarget::~EGLTarget()
{
    ipcClient.deinitialize();

#ifdef BACKEND_BCM_NEXUS_NXCLIENT
    NEXUS_SurfaceClient_Release(reinterpret_cast<NEXUS_SurfaceClient*>(nativeWindow));
#endif
}

void EGLTarget::initialize(uint32_t width, uint32_t height)
{
    if (nativeWindow)
        return;

    uint32_t nexusClientId(0); // For now we only accept 0. See Mail David Montgomery

    if (g_client.ID() != uint8_t(~0)){
        nexusClientId = g_client.ID();
    }

    NXPL_NativeWindowInfo windowInfo;
    windowInfo.x = 0;
    windowInfo.y = 0;
    windowInfo.width = width;
    windowInfo.height = height;
    windowInfo.stretch = false;
#ifdef BACKEND_BCM_NEXUS_NXCLIENT
    windowInfo.zOrder = 0;
#endif
    windowInfo.clientID = nexusClientId;
    nativeWindow = NXPL_CreateNativeWindow(&windowInfo);

    this->width = width;
    this->height = height;
}

void EGLTarget::handleMessage(char* data, size_t size)
{
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    switch (message.messageCode) {
    case IPC::BCMNexus::FrameComplete::code:
    {
        wpe_renderer_backend_egl_target_dispatch_frame_complete(target);
        break;
    }
    default:
        fprintf(stderr, "EGLTarget: unhandled message\n");
    };
}

} // namespace BCMNexus

extern "C" {

struct wpe_renderer_backend_egl_interface bcm_nexus_renderer_backend_egl_interface = {
    // create
    [](int) -> void*
    {
        return new BCMNexus::Backend;
    },
    // destroy
    [](void* data)
    {
        auto* backend = static_cast<BCMNexus::Backend*>(data);
        delete backend;
    },
    // get_native_display
    [](void* data) -> EGLNativeDisplayType
    {
        return EGL_DEFAULT_DISPLAY;
    },
};

struct wpe_renderer_backend_egl_target_interface bcm_nexus_renderer_backend_egl_target_interface = {
    // create
    [](struct wpe_renderer_backend_egl_target* target, int host_fd) -> void*
    {
        return new BCMNexus::EGLTarget(target, host_fd);
    },
    // destroy
    [](void* data)
    {
        auto* target = static_cast<BCMNexus::EGLTarget*>(data);
        delete target;
    },
    // initialize
    [](void* data, void* backend_data, uint32_t width, uint32_t height)
    {
        auto& target = *static_cast<BCMNexus::EGLTarget*>(data);
        target.initialize(width, height);
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType
    {
        auto& target = *static_cast<BCMNexus::EGLTarget*>(data);
        return target.nativeWindow;
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
        auto& target = *static_cast<BCMNexus::EGLTarget*>(data);

        IPC::Message message;
        IPC::BCMNexus::BufferCommit::construct(message, target.width, target.height);
        target.ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
    },
};

struct wpe_renderer_backend_egl_offscreen_target_interface bcm_nexus_renderer_backend_egl_offscreen_target_interface = {
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
        return nullptr;
    },
};

}
