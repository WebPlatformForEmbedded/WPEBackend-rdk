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

#include <wpe/wpe.h>

#include <cstdio>
#include <cstring>

#ifdef BACKEND_BCM_NEXUS
#include "bcm-nexus/interfaces.h"
#endif

#ifdef BACKEND_BCM_NEXUS_WAYLAND
#include "bcm-nexus-wayland/interfaces.h"
#endif

#ifdef BACKEND_BCM_RPI
#include "bcm-rpi/interfaces.h"
#endif

#ifdef BACKEND_INTELCE
#include "intelce/interfaces.h"
#endif

#ifdef BACKEND_WAYLAND_EGL
#include "wayland-egl/interfaces.h"
#endif

#ifdef BACKEND_WESTEROS
#include "westeros/interfaces.h"
#endif

#ifdef BACKEND_VIV_IMX6_EGL
#include "viv-imx6/interfaces.h"
#endif

#ifdef BACKEND_THUNDER
#include "thunder/interfaces.h"
#endif

#ifdef BACKEND_ESSOS
#include "essos/interfaces.h"
#endif

#ifdef BACKEND_HEADLESS
#include "headless/interfaces.h"
#endif

extern "C" {

struct wpe_renderer_host_interface noop_renderer_host_interface = {
    // create
    []() -> void*
    {
        return nullptr;
    },
    // destroy
    [](void* data)
    {
    },
    // create_client
    [](void* data) -> int
    {
        return -1;
    },
};

__attribute__((visibility("default")))
struct wpe_loader_interface _wpe_loader_interface = {
    [](const char* object_name) -> void* {

        if (!std::strcmp(object_name, "_wpe_renderer_host_interface"))
            return &noop_renderer_host_interface;

#ifdef BACKEND_HEADLESS
        static bool is_headless = std::strstr(wpe_loader_get_loaded_implementation_library_name(), "headless") != nullptr;
        if (is_headless && !std::strcmp(object_name, "_wpe_view_backend_interface"))
            return &headless_view_backend_interface;
        if (is_headless && (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface")
                            || !std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface")
                            || !std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface")))
            return nullptr;
#endif

#ifdef BACKEND_BCM_NEXUS
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &bcm_nexus_renderer_backend_egl_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &bcm_nexus_renderer_backend_egl_target_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface"))
            return &bcm_nexus_renderer_backend_egl_offscreen_target_interface;

        if (!std::strcmp(object_name, "_wpe_view_backend_interface"))
            return &bcm_nexus_view_backend_interface;
#endif

#ifdef BACKEND_BCM_NEXUS_WAYLAND
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &bcm_nexus_wayland_renderer_backend_egl_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &bcm_nexus_wayland_renderer_backend_egl_target_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface"))
            return &bcm_nexus_wayland_renderer_backend_egl_offscreen_target_interface;

        if (!std::strcmp(object_name, "_wpe_view_backend_interface"))
            return &bcm_nexus_wayland_view_backend_interface;
#endif

#ifdef BACKEND_BCM_RPI
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &bcm_rpi_renderer_backend_egl_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &bcm_rpi_renderer_backend_egl_target_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface"))
            return &bcm_rpi_renderer_backend_egl_offscreen_target_interface;

        if (!std::strcmp(object_name, "_wpe_view_backend_interface"))
            return &bcm_rpi_view_backend_interface;
#endif

#ifdef BACKEND_INTELCE
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &intelce_renderer_backend_egl_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &intelce_renderer_backend_egl_target_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface"))
            return &intelce_renderer_backend_egl_offscreen_target_interface;

        if (!std::strcmp(object_name, "_wpe_view_backend_interface"))
            return &intelce_view_backend_interface;
#endif

#ifdef BACKEND_WAYLAND_EGL
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &wayland_egl_renderer_backend_egl_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &wayland_egl_renderer_backend_egl_target_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface"))
            return &wayland_egl_renderer_backend_egl_offscreen_target_interface;

        if (!std::strcmp(object_name, "_wpe_view_backend_interface"))
            return &wayland_egl_view_backend_interface;
#endif

#ifdef BACKEND_WESTEROS
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &westeros_renderer_backend_egl_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &westeros_renderer_backend_egl_target_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface"))
            return &westeros_renderer_backend_egl_offscreen_target_interface;

        if (!std::strcmp(object_name, "_wpe_view_backend_interface"))
            return &westeros_view_backend_interface;
#endif

#ifdef BACKEND_VIV_IMX6_EGL
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &viv_imx6_renderer_backend_egl_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &viv_imx6_renderer_backend_egl_target_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface"))
            return &viv_imx6_renderer_backend_egl_offscreen_target_interface;

        if (!std::strcmp(object_name, "_wpe_view_backend_interface"))
            return &viv_imx6_view_backend_interface;
#endif

#ifdef BACKEND_THUNDER
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &thunder_renderer_backend_egl_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &thunder_renderer_backend_egl_target_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface"))
            return &thunder_renderer_backend_egl_offscreen_target_interface;

        if (!std::strcmp(object_name, "_wpe_view_backend_interface"))
            return &thunder_view_backend_interface;
#endif

#ifdef BACKEND_ESSOS
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &essos_renderer_backend_egl_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &essos_renderer_backend_egl_target_interface;
        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface"))
            return &essos_renderer_backend_egl_offscreen_target_interface;

        if (!std::strcmp(object_name, "_wpe_view_backend_interface"))
            return &essos_view_backend_interface;
#endif

        return nullptr;
    },
};

}
