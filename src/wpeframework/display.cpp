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
#include <KeyMapper/KeyMapper.h>
#include <cstring>
#include <chrono>

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

namespace {
    inline uint32_t TimeNow()
    {
        return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }
}

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

    // Send the event, it is complete..
    _callback->Key(action == IKeyboard::pressed, WPE::KeyMapper::KeyCodeToWpeKey(key), key, _modifiers, time);
}

/* virtual */ void KeyboardHandler::Direct(const uint32_t key, const Compositor::IDisplay::IKeyboard::state action)
{
    _callback->Key(key, action);
}

/* virtual */ void KeyboardHandler::KeyMap(const char information[], const uint16_t size) {
}

/* virtual */ void KeyboardHandler::Key(const uint32_t key, const IKeyboard::state action, const uint32_t time) {
    // IDK.
    HandleKeyEvent(key, action, time);

    if (_repeatInfo.rate != 0) {
        if (action == IKeyboard::released && _repeatData.key == key) {
            if (_repeatData.eventSource)
                g_source_remove(_repeatData.eventSource);
            _repeatData = { 0, 0, IKeyboard::released, 0 };
        }
        else if (action == IKeyboard::pressed) {
        // TODO: add code to handle repeat key
        }
    }
}

/* virtual */ void KeyboardHandler::Modifiers(uint32_t depressedMods, uint32_t latchedMods, uint32_t lockedMods, uint32_t group) {
  unsigned int modifiers = 0;

  if (depressedMods & 1)
    modifiers |= wpe_input_keyboard_modifier_shift;
  if (depressedMods & 4)
    modifiers |= wpe_input_keyboard_modifier_control;
  if (depressedMods & 8)
    modifiers |= wpe_input_keyboard_modifier_alt;

  _modifiers = modifiers;
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
// Wheel handling
// -----------------------------------------------------------------------------------------

/* virtual */ void WheelHandler::Direct(const int16_t horizontal, const int16_t vertical)
{
    _callback->WheelMotion(horizontal, vertical);
}

// -----------------------------------------------------------------------------------------
// Pointer handling
// -----------------------------------------------------------------------------------------

/* virtual */  void PointerHandler::Direct(const uint8_t button, const IPointer::state state)
{
    _button = (button + 1);

    uint32_t modifiers = 0;
    switch (_button) {
    case 1:
        modifiers = wpe_input_pointer_modifier_button1;
        break;
    case 2:
        modifiers = wpe_input_pointer_modifier_button2;
        break;
    case 3:
        modifiers = wpe_input_pointer_modifier_button3;
        break;
    }

    _state = (state == Compositor::IDisplay::IPointer::pressed);
    if (_state) {
        _modifiers |= modifiers;
    } else {
        _modifiers &= ~modifiers;
    }

    _callback->PointerButton(_button, _state, _x, _y, _modifiers);
}

/* virtual */  void PointerHandler::Direct(const uint16_t x, const uint16_t y)
{
    _x = x;
    _y = y;
    _callback->PointerPosition(_button, _state, _x, _y, _modifiers);
}

// -----------------------------------------------------------------------------------------
// Touch panel handling
// -----------------------------------------------------------------------------------------

/* virtual */  void TouchPanelHandler::Direct(const uint8_t index, const ITouchPanel::state state, const uint16_t x, const uint16_t y)
{
    _callback->Touch(index, state, x, y);
}

// -----------------------------------------------------------------------------------------
// Display wrapper around the wayland abstraction class
// -----------------------------------------------------------------------------------------
Display::Display(IPC::Client& ipc, const std::string& name)
    : m_ipc(ipc)
    , m_eventSource(g_source_new(&EventSource::sourceFuncs, sizeof(EventSource)))
    , m_keyboard(this)
    , m_wheel(this)
    , m_pointer(this)
    , m_touchpanel(this)
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
    uint32_t actual_key = keycode + 8;
    uint32_t modifiers = 0;
    struct wpe_input_keyboard_event event{ TimeNow(), WPE::KeyMapper::KeyCodeToWpeKey(keycode), actual_key, !!actions, modifiers };
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

void Display::WheelMotion(const int16_t horizontal, const int16_t vertical)
{
    const int Y_AXIS = 0;
    const int X_AXIS = 1;

    wpe_input_axis_event event{};
    event.type = wpe_input_axis_event_type_motion;
    event.time = TimeNow();

    if (horizontal != 0) {
        event.axis = X_AXIS;
        event.value = horizontal;
    } else if (vertical != 0) {
        event.axis = Y_AXIS;
        event.value = vertical;
    }

    SendEvent(event);
}

void Display::PointerButton(const uint8_t button, const uint16_t state, const uint16_t x, const uint16_t y, const uint32_t modifiers)
{
    wpe_input_pointer_event event { wpe_input_pointer_event_type_button, TimeNow(), x, y, button, state, modifiers };
    SendEvent(event);
}

void Display::PointerPosition(const uint8_t button, const uint16_t state, const uint16_t x, const uint16_t y, const uint32_t modifiers)
{
    wpe_input_pointer_event event = { wpe_input_pointer_event_type_motion, TimeNow(), x, y, button, state, modifiers };
    SendEvent(event);
}

void Display::Touch(const uint8_t index, const Compositor::IDisplay::ITouchPanel::state state, const uint16_t x, const uint16_t y)
{
    wpe_input_touch_event_type type = ((state == Compositor::IDisplay::ITouchPanel::motion)? wpe_input_touch_event_type_motion
                                        : ((state == Compositor::IDisplay::ITouchPanel::pressed)? wpe_input_touch_event_type_down
                                            : wpe_input_touch_event_type_up));

    wpe_input_touch_event_raw touchpoint = { type, TimeNow(), index, x, y };
    SendEvent(touchpoint);
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

void Display::SendEvent(wpe_input_touch_event_raw& event)
{
    IPC::Message message;
    message.messageCode = MsgType::TOUCHSIMPLE;
    std::memcpy(message.messageData, &event, sizeof(event));
    m_ipc.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

} // namespace WPEFramework
