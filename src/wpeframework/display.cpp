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

#include "display.h"

namespace WPEFramework {

// -----------------------------------------------------------------------------------------
// GLIB framework thread, to keep the wayland loop a-live
// -----------------------------------------------------------------------------------------
class EventSource {
public:
    static GSourceFuncs sourceFuncs;

    GSource source;
    GPollFD pfd;
    Wayland::Display* display;
};

GSourceFuncs EventSource::sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        EventSource& source (*(reinterpret_cast<EventSource*>(base)));
        // struct wl_display* display = source->display;

        *timeout = -1;

        // while (wl_display_prepare_read(display) != 0) {
        //     if (wl_display_dispatch_pending(display) < 0) {
        //        fprintf(stderr, "Wayland::Display: error in wayland prepare\n");
        //        return FALSE;
        //    }
        //}
        m_display.Flush();

        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        EventSource& source (*(reinterpret_cast<EventSource*>(base)));
        // struct wl_display* display = source->display;

        if (source.pfd.revents & G_IO_IN) {
            //if (wl_display_read_events(display) < 0) {
            //    fprintf(stderr, "Wayland::Display: error in wayland read\n");
            //    return FALSE;
            //}
            return TRUE;
        } else {
            //wl_display_cancel_read(display);
            return FALSE;
        }
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        EventSource& source (*(reinterpret_cast<EventSource*>(base)));

        if (source.pfd.revents & G_IO_IN) {
            if (m_display.Process() < 0) {
                fprintf(stderr, "Wayland::Display: error in wayland dispatch\n");
                return G_SOURCE_REMOVE;
            }
        }

        if (source.pfd.revents & (G_IO_ERR | G_IO_HUP))
            return G_SOURCE_REMOVE;

        source->pfd.revents = 0;
        return G_SOURCE_CONTINUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

// -----------------------------------------------------------------------------------------
// XKB Keyboard implementation to be hooked up to the wayland abstraction class
// -----------------------------------------------------------------------------------------
static gboolean repeatDelayTimeout(void* data)
{
    static_cast<KeyboardHandler*>(data)->RepeatDelayTimeout();

    return G_SOURCE_REMOVE;
}

static gboolean repeatRateTimeout(void* data)
{
    static_cast<Display*>(data)->RepeatKeyEvent();
    return G_SOURCE_CONTINUE;
}

void KeyboardHandler::RepeatKeyEvent()
{
    HandleKeyEvent(repeatData.key, repeatData.state, repeatData.time);
}

void KeyboardHandler::RepeatDelayTimeout() {
    RepeatKeyEvent();
    repeatData.eventSource = g_timeout_add(repeatInfo.rate, static_cast<GSourceFunc>(repeatRateTimeout), this);
}

void KeyboardHandler::HandleKeyEvent(const uint32_t key, const uint32_t state, const uint32_t time) {
    uint32_t keysym = xkb_state_key_get_one_sym(m_xkb.state, key);
    uint32_t unicode = xkb_state_key_get_utf32(m_xkb.state, key);

    if (m_xkb.composeState
        && state == WL_KEYBOARD_KEY_STATE_PRESSED
        && xkb_compose_state_feed(m_xkb.composeState, keysym) == XKB_COMPOSE_FEED_ACCEPTED
        && xkb_compose_state_get_status(m_xkb.composeState) == XKB_COMPOSE_COMPOSED)
    {
        keysym = xkb_compose_state_get_one_sym(m_xkb.composeState);
        unicode = xkb_keysym_to_utf32(keysym);
    }

    // Send the event, it is complete..
    _callback->Key(!!state, keysym, unicode, m_xkb.modifiers, time);
}

/* virtual */ void KeyboardHandler::KeyMap(const char information[], const uint16 size) {
    xkb.keymap = xkb_keymap_new_from_string(xkb.context, information, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

    if (!xkb.keymap)
        return;

    xkb.state = xkb_state_new(xkb.keymap);
    if (!xkb.state)
        return;

    xkb.indexes.control = xkb_keymap_mod_get_index(xkb.keymap, XKB_MOD_NAME_CTRL);
    xkb.indexes.alt = xkb_keymap_mod_get_index(xkb.keymap, XKB_MOD_NAME_ALT);
    xkb.indexes.shift = xkb_keymap_mod_get_index(xkb.keymap, XKB_MOD_NAME_SHIFT);
}

/* virtual */ void KeyboardHandler::Key(const uint32_t key, const uint32_t state, const uint32_t time) {
    // IDK.
    uint32_t actual_key = key + 8;
    handleKeyEvent(actual_key, state, time);

    if (repeatInfo.rate != 0) {

        if (state == WL_KEYBOARD_KEY_STATE_RELEASED && repeatData.key == actual_key) {
            if (repeatData.eventSource)
                g_source_remove(repeatData.eventSource);
            repeatData = { 0, 0, 0, 0 };
        } else if (state == WL_KEYBOARD_KEY_STATE_PRESSED
            && xkb_keymap_key_repeats(xkb.keymap, actual_key)) {

            if (repeatData.eventSource)
                g_source_remove(repeatData.eventSource);

            repeatData = { actual_key, time, state, g_timeout_add(repeatInfo.delay, static_cast<GSourceFunc>(repeatDelayTimeout), this) };
        }
    }
}
 
/* virtual */ void KeyboardHandler::Modifiers(uint32_t depressedMods, uint32_t latchedMods, uint32_t lockedMods, uint32_t group) {
   xkb_state_update_mask(xkb.state, depressedMods, latchedMods, lockedMods, 0, 0, group);

   xkb.modifiers = 0;
   auto component = static_cast<xkb_state_component>(XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);
   if (xkb_state_mod_index_is_active(xkb.state, xkb.indexes.control, component))
       xkb.modifiers |= control;
   if (xkb_state_mod_index_is_active(xkb.state, xkb.indexes.alt, component))
       xkb.modifiers |= alternate;
   if (xkb_state_mod_index_is_active(xkb.state, xkb.indexes.shift, component))
       xkb.modifiers |= shift;
}

/* virtual */ void KeyboardHandler::Repeat(int32_t rate, int32_t delay) {
    repeatInfo = { rate, delay };

    // A rate of zero disables any repeating.
    if (!rate) {
        if (repeatData.eventSource) {
            g_source_remove(repeatData.eventSource);
            repeatData = { 0, 0, 0, 0 };
        }
    }
}

// -----------------------------------------------------------------------------------------
// Display wrapper around the wayland abstraction class
// -----------------------------------------------------------------------------------------
Display::Display(IPC::Client& ipc, struct wpe_view_backend* backend) 
    : m_ipc(ipc)
    , m_eventSource(g_source_new(&EventSource::sourceFuncs, sizeof(EventSource)))
    , m_display(Wayland::Display::Instance())
    , m_keyboard(this)
    , m_backend(backend) {

    EventSource* source (reinterpret_cast<EventSource*>(m_eventSource));

    source->display = &m_display;
    source->pfd.fd = wl_display_get_fd(m_display);
    source->pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
    source->pfd.revents = 0;
    g_source_add_poll(m_eventSource, &source->pfd);
    g_source_set_name(m_eventSource, "[WPE] Display");
    g_source_set_priority(m_eventSource, G_PRIORITY_HIGH + 30);
    g_source_set_can_recurse(m_eventSource, TRUE);
    g_source_attach(m_eventSource, g_main_context_get_thread_default());
}

Display::~Display() {
}

/* virtual */ void Display::Key (const bool pressed, uint32_t keycode, uint32_t unicode, uint32_t modifiers, uint32_t time) override;
{
    if ( m_ipc != nullptr )
    {
        uint8_t wpe_modifiers = 0;
        if ((modifiers & KeyboardHandler::control) != 0)
            wpe_modifiers |= wpe_input_keyboard_modifier_control;
        if ((modifiers & KeyboardHandler::alternate) != 0)
            wpe_modifiers |= wpe_input_keyboard_modifier_alt;
        if ((modifiers & KeyboardHandler::shift) != 0)
            wpe_modifiers |= wpe_input_keyboard_modifier_shift;

        struct wpe_input_keyboard_event event = { time, keycode, unicode, pressed, wpe_modifiers };

        IPC::Message message;
        message.messageCode = MsgType::KEYBOARD;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);

        wpe_view_backend_dispatch_keyboard_event(m_backend, &event);
    }
}

void Display::SendEvent( wpe_input_axis_event& event )
{
    if ( m_ipc != nullptr )
    {
        IPC::Message message;
        message.messageCode = MsgType::AXIS;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void Display::SendEvent( wpe_input_pointer_event& event )
{
    if ( m_ipc != nullptr )
    {
        IPC::Message message;
        message.messageCode = MsgType::POINTER;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void Display::SendEvent( wpe_input_touch_event& event )
{
    if ( m_ipc != nullptr )
    {
        IPC::Message message;
        message.messageCode = MsgType::TOUCH;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

/* If we have pointer and or touch support in the abstraction layer, link it through like here 
static const struct wl_pointer_listener g_pointerListener = {
    // enter
    [](void* data, struct wl_pointer*, uint32_t serial, struct wl_surface* surface, wl_fixed_t, wl_fixed_t)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);
        seatData.serial = serial;
        auto it = seatData.inputClients.find(surface);
        if (it != seatData.inputClients.end())
            seatData.pointer.target = *it;
    },
    // leave
    [](void* data, struct wl_pointer*, uint32_t serial, struct wl_surface* surface)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);
        seatData.serial = serial;
        auto it = seatData.inputClients.find(surface);
        if (it != seatData.inputClients.end() && seatData.pointer.target.first == it->first)
            seatData.pointer.target = { nullptr, nullptr };
    },
    // motion
    [](void* data, struct wl_pointer*, uint32_t time, wl_fixed_t fixedX, wl_fixed_t fixedY)
    {
        auto x = wl_fixed_to_int(fixedX);
        auto y = wl_fixed_to_int(fixedY);

        auto& pointer = static_cast<Display::SeatData*>(data)->pointer;
        pointer.coords = { x, y };

        struct wpe_input_pointer_event event = { wpe_input_pointer_event_type_motion, time, x, y, pointer.button, pointer.state };
        EventDispatcher::singleton().sendEvent( event );

        if (pointer.target.first) {
            struct wpe_view_backend* backend = pointer.target.second;
            wpe_view_backend_dispatch_pointer_event(backend, &event);
        }
    },
    // button
    [](void* data, struct wl_pointer*, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
    {
        static_cast<Display::SeatData*>(data)->serial = serial;

        if (button >= BTN_MOUSE)
            button = button - BTN_MOUSE + 1;
        else
            button = 0;

        auto& pointer = static_cast<Display::SeatData*>(data)->pointer;
        auto& coords = pointer.coords;

        pointer.button = !!state ? button : 0;
        pointer.state = state;

        struct wpe_input_pointer_event event = { wpe_input_pointer_event_type_button, time, coords.first, coords.second, button, state };
        EventDispatcher::singleton().sendEvent( event );

        if (pointer.target.first) {
            struct wpe_view_backend* backend = pointer.target.second;
            wpe_view_backend_dispatch_pointer_event(backend, &event);
        }
    },
    // axis
    [](void* data, struct wl_pointer*, uint32_t time, uint32_t axis, wl_fixed_t value)
    {
        auto& pointer = static_cast<Display::SeatData*>(data)->pointer;
        auto& coords = pointer.coords;

        struct wpe_input_axis_event event = { wpe_input_axis_event_type_motion, time, coords.first, coords.second, axis, -wl_fixed_to_int(value) };
        EventDispatcher::singleton().sendEvent( event );

        if (pointer.target.first) {
            struct wpe_view_backend* backend = pointer.target.second;
            wpe_view_backend_dispatch_axis_event(backend, &event);
        }
    },
};


static const struct wl_touch_listener g_touchListener = {
    // down
    [](void* data, struct wl_touch*, uint32_t serial, uint32_t time, struct wl_surface* surface, int32_t id, wl_fixed_t x, wl_fixed_t y)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);
        seatData.serial = serial;

        int32_t arraySize = std::tuple_size<decltype(seatData.touch.targets)>::value;
        if (id < 0 || id >= arraySize)
            return;

        auto& target = seatData.touch.targets[id];
        assert(!target.first && !target.second);

        auto it = seatData.inputClients.find(surface);
        if (it == seatData.inputClients.end())
            return;

        target = { surface, it->second };

        auto& touchPoints = seatData.touch.touchPoints;
        touchPoints[id] = { wpe_input_touch_event_type_down, time, id, wl_fixed_to_int(x), wl_fixed_to_int(y) };

        struct wpe_input_touch_event event = { touchPoints.data(), touchPoints.size(), wpe_input_touch_event_type_down, id, time };

        struct wpe_view_backend* backend = target.second;
        wpe_view_backend_dispatch_touch_event(backend, &event);
    },
    // up
    [](void* data, struct wl_touch*, uint32_t serial, uint32_t time, int32_t id)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);
        seatData.serial = serial;

        int32_t arraySize = std::tuple_size<decltype(seatData.touch.targets)>::value;
        if (id < 0 || id >= arraySize)
            return;

        auto& target = seatData.touch.targets[id];
        assert(target.first && target.second);

        auto& touchPoints = seatData.touch.touchPoints;
        auto& point = touchPoints[id];
        point = { wpe_input_touch_event_type_up, time, id, point.x, point.y };

        struct wpe_input_touch_event event = { touchPoints.data(), touchPoints.size(), wpe_input_touch_event_type_up, id, time };

        struct wpe_view_backend* backend = target.second;
        wpe_view_backend_dispatch_touch_event(backend, &event);

        point = { wpe_input_touch_event_type_null, 0, 0, 0, 0 };
        target = { nullptr, nullptr };
    },
    // motion
    [](void* data, struct wl_touch*, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);

        int32_t arraySize = std::tuple_size<decltype(seatData.touch.targets)>::value;
        if (id < 0 || id >= arraySize)
            return;

        auto& target = seatData.touch.targets[id];
        assert(target.first && target.second);

        auto& touchPoints = seatData.touch.touchPoints;
        touchPoints[id] = { wpe_input_touch_event_type_motion, time, id, wl_fixed_to_int(x), wl_fixed_to_int(y) };

        struct wpe_input_touch_event event = { touchPoints.data(), touchPoints.size(), wpe_input_touch_event_type_motion, id, time };

        struct wpe_view_backend* backend = target.second;
        wpe_view_backend_dispatch_touch_event(backend, &event);
    },
    // frame
    [](void*, struct wl_touch*)
    {
        // FIXME: Dispatching events via frame() would avoid dispatching events
        // for every single event that's encapsulated in a frame with multiple
        // other events.
    },
    // cancel
    [](void*, struct wl_touch*) { },
};
*/

} // namespace WPEFramework
