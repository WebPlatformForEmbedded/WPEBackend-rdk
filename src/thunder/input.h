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

#pragma once

#include "ipc.h"

#include <assert.h>
#include <compositor/Client.h>
#include <wpe/wpe.h>

struct wpe_view_backend;

typedef struct _GSource GSource;

namespace Thunder {

class KeyboardHandler : public Compositor::IDisplay::IKeyboard {
private:
    KeyboardHandler() = delete;
    KeyboardHandler(const KeyboardHandler&) = delete;
    KeyboardHandler& operator=(const KeyboardHandler&) = delete;

public:
    enum modifiers {
        shift = 0x01,
        control = 0x02,
        alternate = 0x04
    };

    struct IKeyHandler {
        virtual ~IKeyHandler() { }
        virtual void Key(const bool pressed, uint32_t keycode, uint32_t unicode, uint32_t modifiers, uint32_t time) = 0;
        virtual void Key(const uint32_t key, const Compositor::IDisplay::IKeyboard::state action) = 0;
    };

public:
    KeyboardHandler(IKeyHandler* callback)
        : _callback(callback)
        , _modifiers(0)
    {
    }
    virtual ~KeyboardHandler()
    {
    }

public:
    virtual uint32_t AddRef() const
    {
        return (0);
    }
    virtual uint32_t Release() const
    {
        return (0);
    }
    virtual void KeyMap(const char information[], const uint16_t size) override;
    virtual void Key(const uint32_t key, const IKeyboard::state action, const uint32_t time) override;
    virtual void Modifiers(uint32_t depressedMods, uint32_t latchedMods, uint32_t lockedMods, uint32_t group) override;
    virtual void Repeat(int32_t rate, int32_t delay) override;
    virtual void Direct(const uint32_t key, const Compositor::IDisplay::IKeyboard::state action) override;

    void RepeatKeyEvent();
    void RepeatDelayTimeout();
    void HandleKeyEvent(const uint32_t key, const IKeyboard::state action, const uint32_t time);

private:
    IKeyHandler* _callback;
    uint32_t _modifiers;
    struct {
        int32_t rate;
        int32_t delay;
    } _repeatInfo { 0, 0 };
    struct {
        uint32_t key;
        uint32_t time;
        IKeyboard::state state;
        uint32_t eventSource;
    } _repeatData { 0, 0, IKeyboard::released, 0 };
};

class WheelHandler : public Compositor::IDisplay::IWheel {
public:
    WheelHandler() = delete;
    WheelHandler(const WheelHandler&) = delete;
    WheelHandler& operator=(const WheelHandler&) = delete;

    struct IWheelMotionHandler {
        virtual ~IWheelMotionHandler() { }
        virtual void WheelMotion(const int16_t horizontal, const int16_t vertical) = 0;
    };

    WheelHandler(IWheelMotionHandler* callback)
        : _callback(callback)
    {
    }
    ~WheelHandler() { }

    uint32_t AddRef() const override { return (0); }
    uint32_t Release() const override { return (0); }

    void Direct(const int16_t horizontal, const int16_t vertical) override;

private:
    IWheelMotionHandler* _callback;
};

class PointerHandler : public Compositor::IDisplay::IPointer {
public:
    PointerHandler() = delete;
    PointerHandler(const PointerHandler&) = delete;
    PointerHandler& operator=(const PointerHandler&) = delete;

    struct IPointerEventHandler {
        virtual ~IPointerEventHandler() { }
        virtual void PointerButton(const uint8_t button, const uint16_t state, const uint16_t x, const uint16_t y, const uint32_t modifiers) = 0;
        virtual void PointerPosition(const uint8_t button, const uint16_t state, const uint16_t x, const uint16_t y, const uint32_t modifiers) = 0;
    };

    PointerHandler(IPointerEventHandler* callback)
        : _callback(callback)
        , _x(0)
        , _y(0)
        , _button(0)
        , _modifiers(0)
    {
    }
    ~PointerHandler() { }

    uint32_t AddRef() const override { return (0); }
    uint32_t Release() const override { return (0); }

    void Direct(const uint8_t button, const IPointer::state state) override;
    void Direct(const uint16_t x, const uint16_t y) override;

private:
    IPointerEventHandler* _callback;
    uint16_t _x;
    uint16_t _y;
    uint16_t _button;
    uint16_t _state;
    uint32_t _modifiers;
};

class TouchPanelHandler : public Compositor::IDisplay::ITouchPanel {
public:
    TouchPanelHandler() = delete;
    TouchPanelHandler(const TouchPanelHandler&) = delete;
    TouchPanelHandler& operator=(const TouchPanelHandler&) = delete;

    struct ITouchEventHandler {
        virtual ~ITouchEventHandler() { }
        virtual void Touch(const uint8_t index, const ITouchPanel::state state, const uint16_t x, const uint16_t y) = 0;
    };

    TouchPanelHandler(ITouchEventHandler* callback)
        : _callback(callback)
    {
    }
    ~TouchPanelHandler() { }

    uint32_t AddRef() const override { return (0); }
    uint32_t Release() const override { return (0); }

    void Direct(const uint8_t index, const ITouchPanel::state state, const uint16_t x, const uint16_t y) override;

private:
    ITouchEventHandler* _callback;
};

class Input : public KeyboardHandler::IKeyHandler,
              public WheelHandler::IWheelMotionHandler,
              public PointerHandler::IPointerEventHandler,
              public TouchPanelHandler::ITouchEventHandler {
private:
public:
    enum MsgType {
        AXIS = 0x30,
        POINTER,
        TOUCH,
        TOUCHSIMPLE,
        KEYBOARD
    };

public:
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    Input() = delete;

    Input(IPC::Client& ipc);
    ~Input();

public:
    void SendEvent(wpe_input_axis_event& event);
    void SendEvent(wpe_input_pointer_event& event);
    void SendEvent(wpe_input_touch_event& event);
    void SendEvent(wpe_input_touch_event_raw& event);

private:
    virtual void Key(const bool pressed, uint32_t keycode, uint32_t unicode, uint32_t modifiers, uint32_t time) override;
    virtual void Key(const uint32_t key, const Compositor::IDisplay::IKeyboard::state action);
    virtual void WheelMotion(const int16_t horizontal, const int16_t vertical) override;
    virtual void PointerButton(const uint8_t button, const uint16_t state, const uint16_t x, const uint16_t y, const uint32_t modifiers) override;
    virtual void PointerPosition(const uint8_t button, const uint16_t state, const uint16_t x, const uint16_t y, const uint32_t modifiers) override;
    virtual void Touch(const uint8_t index, const Compositor::IDisplay::ITouchPanel::state state, const uint16_t x, const uint16_t y) override;

private:
    IPC::Client& _ipc;
    GSource* _eventSource;
    uint32_t _modifiers;
};

} // namespace Thunder
