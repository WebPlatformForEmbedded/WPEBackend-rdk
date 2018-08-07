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
#include <cstring>

namespace WPEFramework {

// -----------------------------------------------------------------------------------------
// GLIB framework thread, to keep the wayland loop a-live
// -----------------------------------------------------------------------------------------
class EventSource {
public:
    static GSourceFuncs sourceFuncs;

    GSource source;
    GPollFD pfd;
    Compositor::IDisplay* display;
    signed int result;
};

GSourceFuncs EventSource::sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean {
        *timeout = -1;
        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean {
        EventSource& source(*(reinterpret_cast<EventSource*>(base)));

        source.result = source.display->Process(source.pfd.revents & G_IO_IN);

        return (source.result >= 0 ? TRUE : FALSE);
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean {
        EventSource& source(*(reinterpret_cast<EventSource*>(base)));

        if ((source.result == 1) || (source.pfd.revents & (G_IO_ERR | G_IO_HUP))) {
            fprintf(stderr, "Compositor::Display: error in compositor dispatch\n");
            return G_SOURCE_REMOVE;
        }

        source.pfd.revents = 0;
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
    static_cast<KeyboardHandler*>(data)->RepeatKeyEvent();
    return G_SOURCE_CONTINUE;
}

void KeyboardHandler::RepeatKeyEvent()
{
    HandleKeyEvent(_repeatData.key, _repeatData.state, _repeatData.time);
}

void KeyboardHandler::RepeatDelayTimeout() {
    RepeatKeyEvent();
    _repeatData.eventSource = g_timeout_add(_repeatInfo.rate, static_cast<GSourceFunc>(repeatRateTimeout), this);
}

void KeyboardHandler::HandleKeyEvent(const uint32_t key, const IKeyboard::state action, const uint32_t time) {
    uint32_t keysym = wpe_input_xkb_context_get_key_code(wpe_input_xkb_context_get_default(), key, action == IKeyboard::pressed);
    if (!keysym)
	return;

    // Send the event, it is complete..
    _callback->Key(action == IKeyboard::pressed, keysym, key, _modifiers, time);
}

/* virtual */ void KeyboardHandler::Direct(const uint32_t key, const Compositor::IDisplay::IKeyboard::state action)
{
    _callback->Key(key, action);
}

/* virtual */ void KeyboardHandler::KeyMap(const char information[], const uint16_t size) {
    auto* xkb = wpe_input_xkb_context_get_default();
    auto* keymap = xkb_keymap_new_from_string(wpe_input_xkb_context_get_context(xkb), information, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    wpe_input_xkb_context_set_keymap(xkb, keymap);
    xkb_keymap_unref(keymap);
}

/* virtual */ void KeyboardHandler::Key(const uint32_t key, const IKeyboard::state action, const uint32_t time) {
    // IDK.
    uint32_t actual_key = key + 8;
    HandleKeyEvent(actual_key, action, time);

    auto* keymap = wpe_input_xkb_context_get_keymap(wpe_input_xkb_context_get_default());

    if (_repeatInfo.rate != 0) {
        if (action == IKeyboard::released && _repeatData.key == actual_key) {
            if (_repeatData.eventSource)
                g_source_remove(_repeatData.eventSource);
            _repeatData = { 0, 0, IKeyboard::released, 0 };
        }
        else if (action == IKeyboard::pressed
            && keymap && xkb_keymap_key_repeats(keymap, actual_key)) {

            if (_repeatData.eventSource)
                g_source_remove(_repeatData.eventSource);

            _repeatData = { actual_key, time, action, g_timeout_add(_repeatInfo.delay, static_cast<GSourceFunc>(repeatDelayTimeout), this) };
        }
    }
}

/* virtual */ void KeyboardHandler::Modifiers(uint32_t depressedMods, uint32_t latchedMods, uint32_t lockedMods, uint32_t group) {
    _modifiers = wpe_input_xkb_context_get_modifiers(wpe_input_xkb_context_get_default(), depressedMods, latchedMods, lockedMods, group);
}

/* virtual */ void KeyboardHandler::Repeat(int32_t rate, int32_t delay) {
    _repeatInfo = { rate, delay };

    // A rate of zero disables any repeating.
    if (!rate) {
        if (_repeatData.eventSource) {
            g_source_remove(_repeatData.eventSource);
            _repeatData = { 0, 0, IKeyboard::released, 0 };
        }
    }
}

// -----------------------------------------------------------------------------------------
// Display wrapper around the wayland abstraction class
// -----------------------------------------------------------------------------------------
Display::Display(IPC::Client& ipc, const std::string& name)
    : m_ipc(ipc)
    , m_eventSource(g_source_new(&EventSource::sourceFuncs, sizeof(EventSource)))
    , m_keyboard(this)
    , m_backend(nullptr)
    , m_display(Compositor::IDisplay::Instance(name))
{
    int descriptor = m_display->FileDescriptor();
    EventSource* source(reinterpret_cast<EventSource*>(m_eventSource));

    if (descriptor != -1) {
        source->display = m_display;
        source->pfd.fd = descriptor;
        source->pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
        source->pfd.revents = 0;

        g_source_add_poll(m_eventSource, &source->pfd);
        g_source_set_name(m_eventSource, "[WPE] Display");
        g_source_set_priority(m_eventSource, G_PRIORITY_HIGH + 30);
        g_source_set_can_recurse(m_eventSource, TRUE);
        g_source_attach(m_eventSource, g_main_context_get_thread_default());
    }
}

Display::~Display()
{
}

/* virtual */ void Display::Key (const uint32_t keycode, const Compositor::IDisplay::IKeyboard::state actions) {
    uint32_t actual_key = key + 8;

    auto* xkb = wpe_input_xkb_context_get_default();
    uint32_t keysym = wpe_input_xkb_context_get_key_code(xkb, actual_key, !!actions);
    if (!keysym)
        return;
    auto* xkbState = wpe_input_xkb_context_get_state(xkb);
    xkb_state_update_key(xkbState, actual_key, !!actions);
    uint32_t modifiers = wpe_input_xkb_context_get_modifiers(xkb,
        xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_DEPRESSED),
        xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_LATCHED),
        xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_LOCKED),
        xkb_state_serialize_layout(xkbState, XKB_STATE_LAYOUT_EFFECTIVE));
    struct wpe_input_keyboard_event event{ time(nullptr), keysym, actual_key, !!actions, modifiers };
    IPC::Message message;
    message.messageCode = MsgType::KEYBOARD;
    std::memcpy(message.messageData, &event, sizeof(event));
    m_ipc.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

/* virtual */ void Display::Key(const bool pressed, uint32_t keycode, uint32_t hardware_keycode, uint32_t modifiers, uint32_t time)
{
    struct wpe_input_keyboard_event event = { time, keycode, hardware_keycode, pressed, modifiers };

    IPC::Message message;
    message.messageCode = MsgType::KEYBOARD;
    std::memcpy(message.messageData, &event, sizeof(event));
    m_ipc.sendMessage(IPC::Message::data(message), IPC::Message::size);
    // TODO: this is not needed but it was done in the wayland-egl code, lets remove this later.
    // wpe_view_backend_dispatch_keyboard_event(m_backend, &event);
}

void Display::SendEvent(wpe_input_axis_event& event)
{
    IPC::Message message;
    message.messageCode = MsgType::AXIS;
    std::memcpy(message.messageData, &event, sizeof(event));
    m_ipc.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void Display::SendEvent(wpe_input_pointer_event& event)
{
    IPC::Message message;
    message.messageCode = MsgType::POINTER;
    std::memcpy(message.messageData, &event, sizeof(event));
    m_ipc.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void Display::SendEvent(wpe_input_touch_event& event)
{
    IPC::Message message;
    message.messageCode = MsgType::TOUCH;
    std::memcpy(message.messageData, &event, sizeof(event));
    m_ipc.sendMessage(IPC::Message::data(message), IPC::Message::size);
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
