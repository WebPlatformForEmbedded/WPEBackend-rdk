/*
 * Copyright (C) 2015, 2016 Igalia S.L.
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

#ifndef wpe_view_backend_directfb_display_h
#define wpe_view_backend_directfb_display_h

#include <directfb.h>

#define DFB_EVENT_BUFFER_TIMOUT_SEC 0 /* seconds*/
#define DFB_EVENT_BUFFER_TIMOUT_MS 100 /* MiliSeconds*/

#include "dfb_backend_log.h"

#include <array>
#include <unordered_map>
#include <utility>
#include <wpe/wpe.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>
#include "ipc.h"

struct wpe_view_backend;

typedef struct _GSource GSource;

namespace Directfb {

class EventDispatcher
{
public:
    static EventDispatcher& singleton();
    void sendEvent( wpe_input_axis_event& event );
    void sendEvent( wpe_input_pointer_event& event );
    void sendEvent( wpe_input_touch_event& event );
    void sendEvent( wpe_input_keyboard_event& event );
    void sendEvent( wpe_input_touch_event_raw& event );
    void setIPC( IPC::Client& ipcClient );
    enum MsgType
    {
        AXIS = 0,
        POINTER,
        TOUCH,
        TOUCHSIMPLE,
        KEYBOARD
    };
private:
    EventDispatcher() {};
    ~EventDispatcher() {};
    IPC::Client * m_ipc;
};

class Display {
public:
    static Display& singleton();
    void unregisterDirectFBDisplayPlatform();
    bool registerDirectFBDisplayPlatform();
    bool initDirectFB();
    void deInitDirectFB();
    void initializeEventSource(IDirectFBWindow *dfb_window);

    IDirectFB* iDirectfb() const { return m_dfb; }
    unsigned int width() const { return m_layerConfig.width; }
    unsigned int height() const { return m_layerConfig.height; }

    struct SeatData {
        struct {
            struct xkb_keymap* keymap;
            struct xkb_state* state;
            struct {
                xkb_mod_index_t control;
                xkb_mod_index_t alt;
                xkb_mod_index_t shift;
            } indexes;
            uint8_t modifiers;
        } xkb { nullptr, nullptr, { 0, 0, 0 }, 0};
    };

private:
    Display();
    ~Display();

    IDirectFB* m_dfb;
    DFBDisplayLayerConfig m_layerConfig;

    SeatData m_seatData;
    GSource* m_eventSource;
};

} // namespace Directfb

#endif // wpe_view_backend_directfb_display_h

