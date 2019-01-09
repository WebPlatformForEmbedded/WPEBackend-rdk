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

#include "LibinputServer.h"

#include "KeyboardEventRepeating.h"
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#define XKB_ML_KeyRed           0x6d6c0001
#define XKB_ML_KeyGreen         0x6d6c0002
#define XKB_ML_KeyYellow        0x6d6c0003
#define XKB_ML_KeyBlue          0x6d6c0004
#define XKB_ML_KeyChannelUp     0x6d6c0005
#define XKB_ML_KeyChannelDown   0x6d6c0006
#define XKB_ML_KeyPlayPause     0x6d6c0007
#define XKB_ML_KeyRewind        0x6d6c0008
#define XKB_ML_KeyFastForward   0x6d6c0009

namespace WPE {

#ifndef KEY_INPUT_HANDLING_VIRTUAL

struct libinput_interface g_interface = {
    // open_restricted
    [](const char* path, int flags, void*)
    {
        return open(path, flags);
    },
    [](int fd, void*)
    {
        close(fd);
    }
};

#else

static const char * connectorName = "/tmp/keyhandler";

static void VirtualKeyboardCallback(actiontype type , unsigned int code)
{
   if (type != COMPLETED)
   {
      // RELEASED  = 0,
      // PRESSED   = 1,
      // REPEAT    = 2,
      // COMPLETED = 3
      LibinputServer::singleton().VirtualInput (type, code);
   }
}

void LibinputServer::VirtualInput (unsigned int type, unsigned int code)
{
    handleKeyboardEvent(time(nullptr), code + 8, type);
}

#endif

bool LibinputServer::handleKeyboardEvent(uint32_t eventTime, uint32_t code, uint32_t state)
{
    auto* xkb = wpe_input_xkb_context_get_default();

    uint32_t keysym = 0;

    switch (code) {
    case 0x18e: // KEY_RED
        keysym = XKB_ML_KeyRed;
        break;
    case 0x18f: // KEY_GREEN
        keysym = XKB_ML_KeyGreen;
        break;
    case 0x190: // KEY_YELLOW
        keysym = XKB_ML_KeyYellow;
        break;
    case 0x191: // KEY_BLUE
        keysym = XKB_ML_KeyBlue;
        break;
    case 0x192: // KEY_CHANNELUP
        keysym = XKB_ML_KeyChannelUp;
        break;
    case 0x193: // KEY_CHANNELDOWN
        keysym = XKB_ML_KeyChannelDown;
        break;
    case 164: // KEY_PLAYPAUSE
        keysym = XKB_ML_KeyPlayPause;
        break;
    case 168: // KEY_REWIND
        keysym = XKB_ML_KeyRewind;
        break;
    case 208: // KEY_FASTFORWARD
        keysym = XKB_ML_KeyFastForward;
        break;
    default:
        keysym = wpe_input_xkb_context_get_key_code(xkb, code, !!state);
        break;
    }

    if (!keysym)
	return false;

    auto* xkbState = wpe_input_xkb_context_get_state(xkb);
    xkb_state_update_key(xkbState, code, !!state ? XKB_KEY_DOWN : XKB_KEY_UP);
    uint32_t modifiers = wpe_input_xkb_context_get_modifiers(xkb,
        xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_DEPRESSED),
        xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_LATCHED),
        xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_LOCKED),
        xkb_state_serialize_layout(xkbState, XKB_STATE_LAYOUT_EFFECTIVE));
    struct wpe_input_keyboard_event event{ eventTime, keysym, code, !!state, modifiers };
    m_client->handleKeyboardEvent(&event);

    return true;
}

LibinputServer& LibinputServer::singleton()
{
    static LibinputServer server;
    return server;
}

LibinputServer::LibinputServer()
    : m_keyboardEventRepeating(new Input::KeyboardEventRepeating(*this))
    , m_pointerCoords(0, 0)
    , m_pointerBounds(1, 1)
#ifndef KEY_INPUT_HANDLING_VIRTUAL
    , m_udev(nullptr)
#else
    , m_virtualkeyboard(nullptr)
#endif
{
#ifndef KEY_INPUT_HANDLING_VIRTUAL
    m_udev = udev_new();
    if (!m_udev)
        return;

    m_libinput = libinput_udev_create_context(&g_interface, nullptr, m_udev);
    if (!m_libinput)
        return;

    int ret = libinput_udev_assign_seat(m_libinput, "seat0");
    if (ret)
        return;

    GSource* baseSource = g_source_new(&EventSource::s_sourceFuncs, sizeof(EventSource));
    auto* source = reinterpret_cast<EventSource*>(baseSource);
    source->pfd.fd = libinput_get_fd(m_libinput);
    source->pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
    source->pfd.revents = 0;
    g_source_add_poll(baseSource, &source->pfd);
    source->server = this;

    g_source_set_name(baseSource, "[WPE] libinput");
    g_source_set_priority(baseSource, G_PRIORITY_DEFAULT);
    g_source_attach(baseSource, g_main_context_get_thread_default());

    fprintf(stderr, "[LibinputServer] Initialization of linux input system succeeded.\n");

#else

    const char listenerName[] = "WebKitBrowser";
    m_virtualkeyboard = Construct(listenerName, connectorName, VirtualKeyboardCallback);
    if (m_virtualkeyboard == nullptr) {
      fprintf(stderr, "[LibinputServer] Initialization of virtual keyboard failed!!!\n");
    }
    else {
       fprintf(stderr, "[LibinputServer] Initialization of virtual keyboard and linux input system succeeded.\n");
    }
#endif

}

LibinputServer::~LibinputServer()
{
#ifdef KEY_INPUT_HANDLING_VIRTUAL
    if (m_virtualkeyboard != nullptr) {
       Destruct(m_virtualkeyboard);
    }
#else
    libinput_unref(m_libinput);
    if (m_udev) {
        udev_unref(m_udev);
    }
#endif
}

void LibinputServer::setClient(Client* client)
{
    m_client = client;
}

void LibinputServer::handleKeyboardEvent(struct wpe_input_keyboard_event* actionEvent)
{
    if (m_client)
        m_client->handleKeyboardEvent(actionEvent);
}

void LibinputServer::setHandlePointerEvents(bool handle)
{
    m_handlePointerEvents = handle;
    fprintf(stderr, "[LibinputServer] %s pointer events.\n", handle ? "Enabling" : "Disabling");
}

void LibinputServer::setHandleTouchEvents(bool handle)
{
    m_handleTouchEvents = handle;
    fprintf(stderr, "[LibinputServer] %s handle events.\n", handle ? "Enabling" : "Disabling");
}

void LibinputServer::setPointerBounds(uint32_t width, uint32_t height)
{
    m_pointerBounds = { width, height };
}

#ifndef KEY_INPUT_HANDLING_VIRTUAL
void LibinputServer::processEvents()
{
    libinput_dispatch(m_libinput);

    while (auto* event = libinput_get_event(m_libinput)) {
        switch (libinput_event_get_type(event)) {
        case LIBINPUT_EVENT_TOUCH_DOWN: 
            if (m_handleTouchEvents)	
                handleTouchEvent(event, wpe_input_touch_event_type_down);
            break;
        case LIBINPUT_EVENT_TOUCH_UP:	
            if (m_handleTouchEvents)	
                handleTouchEvent(event, wpe_input_touch_event_type_up);    
            break;
        case LIBINPUT_EVENT_TOUCH_MOTION:
            if (m_handleTouchEvents)	
                handleTouchEvent(event, wpe_input_touch_event_type_motion);    
            break;        
        case LIBINPUT_EVENT_KEYBOARD_KEY:
        {
            auto* keyEvent = libinput_event_get_keyboard_event(event);

            auto eventTime = libinput_event_keyboard_get_time(keyEvent);
            auto eventKey = libinput_event_keyboard_get_key(keyEvent) + 8;
            auto eventState = libinput_event_keyboard_get_key_state(keyEvent);

            if (handleKeyboardEvent(eventTime, eventKey, eventState)) {
                if (!!eventState)
                    m_keyboardEventRepeating->schedule(eventTime, eventKey);
                else
                    m_keyboardEventRepeating->cancel();
            }
            break;
        }
        case LIBINPUT_EVENT_POINTER_MOTION:
        {
            if (!m_handlePointerEvents)
                break;

            auto* pointerEvent = libinput_event_get_pointer_event(event);

            double dx = libinput_event_pointer_get_dx(pointerEvent);
            double dy = libinput_event_pointer_get_dy(pointerEvent);
            m_pointerCoords.first = std::min<int32_t>(std::max<uint32_t>(0, m_pointerCoords.first + dx), m_pointerBounds.first - 1);
            m_pointerCoords.second = std::min<int32_t>(std::max<uint32_t>(0, m_pointerCoords.second + dy), m_pointerBounds.second - 1);

            struct wpe_input_pointer_event event{
                wpe_input_pointer_event_type_motion,
                libinput_event_pointer_get_time(pointerEvent),
                m_pointerCoords.first, m_pointerCoords.second, 0, 0, 0
            };
            m_client->handlePointerEvent(&event);
            break;
        }
        case LIBINPUT_EVENT_POINTER_BUTTON:
        {
            if (!m_handlePointerEvents)
                break;

            auto* pointerEvent = libinput_event_get_pointer_event(event);
            struct wpe_input_pointer_event event{
                wpe_input_pointer_event_type_button,
                libinput_event_pointer_get_time(pointerEvent),
                m_pointerCoords.first, m_pointerCoords.second,
                libinput_event_pointer_get_button(pointerEvent),
                libinput_event_pointer_get_button_state(pointerEvent),
                0
            };
            m_client->handlePointerEvent(&event);
            break;
        }
        case LIBINPUT_EVENT_POINTER_AXIS:
        {
            if (!m_handlePointerEvents)
                break;

            auto* pointerEvent = libinput_event_get_pointer_event(event);

            // Support only wheel events for now.
            if (libinput_event_pointer_get_axis_source(pointerEvent) != LIBINPUT_POINTER_AXIS_SOURCE_WHEEL)
                break;

            if (libinput_event_pointer_has_axis(pointerEvent, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
                auto axis = LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL;
                int32_t axisValue = libinput_event_pointer_get_axis_value(pointerEvent, axis);

                struct wpe_input_axis_event event{
                    wpe_input_axis_event_type_motion,
                    libinput_event_pointer_get_time(pointerEvent),
                    m_pointerCoords.first, m_pointerCoords.second,
                    axis, -axisValue, 0
                };
                m_client->handleAxisEvent(&event);
            }

            if (libinput_event_pointer_has_axis(pointerEvent, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
                auto axis = LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL;
                int32_t axisValue = libinput_event_pointer_get_axis_value(pointerEvent, axis);

                struct wpe_input_axis_event event{
                    wpe_input_axis_event_type_motion,
                    libinput_event_pointer_get_time(pointerEvent),
                    m_pointerCoords.first, m_pointerCoords.second,
                    axis, axisValue, 0
                };
                m_client->handleAxisEvent(&event);
            }

            break;
        }
        default:
            break;
        }

        libinput_event_destroy(event);
    }
}

void LibinputServer::handleTouchEvent(struct libinput_event *event, enum wpe_input_touch_event_type type)
{
    auto* touchEvent = libinput_event_get_touch_event(event);
    uint32_t time = libinput_event_touch_get_time(touchEvent);
    int id = libinput_event_touch_get_seat_slot(touchEvent);
    auto& targetPoint = m_touchEvents[id];
    int32_t x, y;
    
    if (type != wpe_input_touch_event_type_up) {
      x = libinput_event_touch_get_x_transformed(touchEvent, m_pointerBounds.first);
      y = libinput_event_touch_get_y_transformed(touchEvent, m_pointerBounds.second);
    } else {
      // libinput can't return pointer position on touch-up
      x = targetPoint.x;
      y = targetPoint.y;
    }
    targetPoint = { type, time, id, x, y };

    struct wpe_input_touch_event dispatchedEvent{ m_touchEvents.data(), m_touchEvents.size(), type, id, time, 0 };
    m_client->handleTouchEvent(&dispatchedEvent);

    if (type == wpe_input_touch_event_type_up) {
        targetPoint = { wpe_input_touch_event_type_null, 0, -1, -1, -1 };
    }
}


GSourceFuncs LibinputServer::EventSource::s_sourceFuncs = {
    nullptr, // prepare
    // check
    [](GSource* base) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        return !!source->pfd.revents;
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);

        if (source->pfd.revents & (G_IO_ERR | G_IO_HUP))
            return FALSE;

        if (source->pfd.revents & G_IO_IN)
            source->server->processEvents();
        source->pfd.revents = 0;
        return TRUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};


#endif

void LibinputServer::dispatchKeyboardEvent(uint32_t eventTime, uint32_t eventKey)
{
#ifndef KEY_INPUT_HANDLING_VIRTUAL
    handleKeyboardEvent(eventTime, eventKey, LIBINPUT_KEY_STATE_PRESSED);
#endif
}

} // namespace WPE
