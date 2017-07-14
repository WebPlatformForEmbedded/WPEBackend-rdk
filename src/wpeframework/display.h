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

#ifndef wpe_view_backend_wpeframework_display_h
#define wpe_view_backend_wpeframework_display_h

#include "ipc.h"

#include <wpe/input.h>
#include <wayland/Client.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>

struct wpe_view_backend;

typedef struct _GSource GSource;

namespace WPEFramework {

class KeyboardHandler : public Wayland::Display::IKeyboard
{
private:
    KeyboardHandler () = delete;
    KeyboardHandler (const KeyboardHandler&) = delete;
    KeyboardHandler& operator= (const KeyboardHandler&) = delete;

public:
    enum modifiers {
       shift     = 0x01,
       control   = 0x02,
       alternate = 0x04
    };

    struct IKeyHandler {
        virtual IKeyHandler() {}
        virtual void Key (const bool pressed, uint32_t keycode, uint32_ti unicode, uint32_ti modifiers, uint32_t) = 0;
    };

public:
    KeyboardHandler (IKeyHandler* callback) : _callback(callback) {
    }
    virtual ~KeyboardHandler() {
    }

public:
    virtual void KeyMap(const char information[], const uint16 size) override;
    virtual void Key(const uint32_t key, const uint32_t state, const uint32_t time) override;
    virtual void Modifiers(uint32_t depressedMods, uint32_t latchedMods, uint32_t lockedMods, uint32_t group) override;
    virtual void Repeat(int32_t rate, int32_t delay) override;

    void RepeatKeyEvent();
    void RepeatDelayTimeout();
    void HandleKeyEvent(const uint32_t key, const uint32_t state, const uint32_t time);

private:
    IKeyHandler* _callback;
    struct {
        struct xkb_context* context;
        struct xkb_keymap* keymap;
        struct xkb_state* state;
        struct {
            xkb_mod_index_t control;
            xkb_mod_index_t alt;
            xkb_mod_index_t shift;
        } indexes;
        uint8_t modifiers;
        struct xkb_compose_table* composeTable;
        struct xkb_compose_state* composeState;
    } m_xkb { nullptr, nullptr, nullptr, { 0, 0, 0 }, 0, nullptr, nullptr };
    struct {
        int32_t rate;
        int32_t delay;
    } repeatInfo { 0, 0 };
    struct {
        uint32_t key;
        uint32_t time;
        uint32_t state;
        uint32_t eventSource;
    } repeatData { 0, 0, 0, 0 };
};

class Display : public KeyboardHandler::IKeyHandler{
private:
    Display() = delete;
    Display (const Display&) = delete;
    Display& operator= (const Display&) = delete;

    enum MsgType
    {
	AXIS = 0x30,
	POINTER,
	TOUCH,
	KEYBOARD
    };

public:
    Display(IPC::Client& ipc, struct wpe_view_backend* backend);
    ~Display();

public:
    inline Wayland::Display::Surface Create(const std::string& name, const uint32_t width, const uint32_t height) {
        Wayland::Display::Surface newSurface = m_display.Create(name, width, height);
        newSurface.Callback(&m_Keyboard);
        return (newSurface);
    }
    void SendEvent( wpe_input_axis_event& event );
    void SendEvent( wpe_input_pointer_event& event );
    void SendEvent( wpe_input_touch_event& event );

private: 
    virtual void Key (const bool pressed, uint32_t keycode, uint32_ti unicode, uint32_ti modifiers, uint32_t) override;

private:
    IPC::Client& m_ipc;
    GSource* m_eventSource;
    Wayland::Display m_display;
    KeyboardHandler m_keyboard;
    struct wpe_view_backend* m_backend;
} // namespace WPEFramework

#endif // wpe_view_backend_wpeframework_display_h
