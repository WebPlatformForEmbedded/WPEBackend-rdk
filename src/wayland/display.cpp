/*
 * Copyright (C) 2015, 2016 Igalia S.L.
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

#ifdef BACKEND_BCM_NEXUS_WAYLAND
#include "nsc-client-protocol.h"
#endif
#include "xdg-shell-client-protocol.h"
#include "wayland-client-protocol.h"
#include <cassert>
#include <cstring>
#include <glib.h>
#include <linux/input.h>
#include <locale.h>
#include <memory>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

namespace Wayland {

class EventSource {
public:
    static GSourceFuncs sourceFuncs;

    GSource source;
    GPollFD pfd;
    struct wl_display* display;
    bool isReading;
};

GSourceFuncs EventSource::sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        auto& source = *reinterpret_cast<EventSource*>(base);

        *timeout = -1;

        if (source.isReading)
            return FALSE;

        // If there are pending dispatches we return TRUE to proceed to dispatching ASAP.
        if (wl_display_prepare_read(source.display) != 0)
            return TRUE;

        source.isReading = true;

        wl_display_flush(source.display);
        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        auto& source = *reinterpret_cast<EventSource*>(base);

        // Only perform the read if input was made available during polling.
        // Error during read is noted and will be handled in the following
        // dispatch callback. If no input is available, the read is canceled.
        if (source.isReading) {
            source.isReading = false;

            if (source.pfd.revents & G_IO_IN) {
                if (wl_display_read_events(source.display) == 0)
                    return TRUE;
            } else
                wl_display_cancel_read(source.display);
        }

        return source.pfd.revents;
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        auto& source = *reinterpret_cast<EventSource*>(base);

        // Remove the source if any error was registered.
        if (source.pfd.revents & (G_IO_ERR | G_IO_HUP))
            return G_SOURCE_REMOVE;

        // Dispatch any pending events. The source is removed in case of
        // an error occurring during this step.
        if (wl_display_dispatch_pending(source.display) < 0)
            return G_SOURCE_REMOVE;

        source.pfd.revents = 0;
        return G_SOURCE_CONTINUE;
    },
    // finalize
    [](GSource* base)
    {
        auto& source = *reinterpret_cast<EventSource*>(base);

        if (source.isReading) {
            wl_display_cancel_read(source.display);
            source.isReading = false;
        }
    },
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

const struct wl_registry_listener g_registryListener = {
    // global
    [](void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
    {
        auto& interfaces = *static_cast<Display::Interfaces*>(data);

        if (!std::strcmp(interface, "wl_compositor"))
            interfaces.compositor = static_cast<struct wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1));

#ifdef BACKEND_BCM_NEXUS_WAYLAND
        if (!std::strcmp(interface, "wl_nsc"))
            interfaces.nsc = static_cast<struct wl_nsc*>(wl_registry_bind(registry, name, &wl_nsc_interface, version));
#endif

        if (!std::strcmp(interface, "wl_seat"))
            interfaces.seat = static_cast<struct wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 4));

        if (!std::strcmp(interface, "xdg_shell"))
            interfaces.xdg = static_cast<struct xdg_shell*>(wl_registry_bind(registry, name, &xdg_shell_interface, 1)); 

        if (!std::strcmp(interface, "wl_shell"))
            interfaces.shell = static_cast<struct wl_shell*>(wl_registry_bind(registry, name, &wl_shell_interface, 1));
    },
    // global_remove
    [](void*, struct wl_registry*, uint32_t) { },
};

static const struct xdg_shell_listener g_xdgShellListener = {
    // ping
    [](void*, struct xdg_shell* shell, uint32_t serial)
    {
        xdg_shell_pong(shell, serial);
    },
};

static uint32_t
getModifiers(Display::SeatData& seatData)
{
    uint32_t mask = seatData.keyboard.modifiers;
    if (seatData.pointer.object)
        mask |= seatData.pointer.modifiers;
    return mask;
}

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

        auto& seatData = *static_cast<Display::SeatData*>(data);
        auto& pointer = seatData.pointer;
        pointer.coords = { x, y };

        struct wpe_input_pointer_event event = { wpe_input_pointer_event_type_motion, time, x, y, pointer.button, pointer.state, getModifiers(seatData) };
        EventDispatcher::singleton().sendEvent( event );

        if (pointer.target.first) {
            struct wpe_view_backend* backend = pointer.target.second;
            wpe_view_backend_dispatch_pointer_event(backend, &event);
        }
    },
    // button
    [](void* data, struct wl_pointer*, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);
        seatData.serial = serial;

        if (button >= BTN_MOUSE)
            button = button - BTN_MOUSE + 1;
        else
            button = 0;

        auto& pointer = seatData.pointer;
        auto& coords = pointer.coords;

        pointer.button = !!state ? button : 0;
        pointer.state = state;

        uint32_t modifier = 0;
        switch (button) {
        case 1:
            modifier = wpe_input_pointer_modifier_button1;
            break;
        case 2:
            modifier = wpe_input_pointer_modifier_button2;
            break;
        case 3:
	    modifier = wpe_input_pointer_modifier_button3;
            break;
        case 4:
            modifier = wpe_input_pointer_modifier_button4;
            break;
        case 5:
            modifier = wpe_input_pointer_modifier_button5;
            break;
        default:
            break;
        }

	if (state)
            seatData.pointer.modifiers |= modifier;
        else
            seatData.pointer.modifiers &= ~modifier;

        struct wpe_input_pointer_event event = { wpe_input_pointer_event_type_button, time, coords.first, coords.second, button, state, getModifiers(seatData) };
        EventDispatcher::singleton().sendEvent( event );

        if (pointer.target.first) {
            struct wpe_view_backend* backend = pointer.target.second;
            wpe_view_backend_dispatch_pointer_event(backend, &event);
        }
    },
    // axis
    [](void* data, struct wl_pointer*, uint32_t time, uint32_t axis, wl_fixed_t value)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);
        auto& pointer = seatData.pointer;
        auto& coords = pointer.coords;

        struct wpe_input_axis_event event = { wpe_input_axis_event_type_motion, time, coords.first, coords.second, axis, -wl_fixed_to_int(value), getModifiers(seatData) };
        EventDispatcher::singleton().sendEvent( event );

        if (pointer.target.first) {
            struct wpe_view_backend* backend = pointer.target.second;
            wpe_view_backend_dispatch_axis_event(backend, &event);
        }
    },
};

static void
handleKeyEvent(Display::SeatData& seatData, uint32_t key, uint32_t state, uint32_t time)
{
    uint32_t keysym = wpe_input_xkb_context_get_key_code(wpe_input_xkb_context_get_default(), key, state == WL_KEYBOARD_KEY_STATE_PRESSED);
    if (!keysym)
	return;

    struct wpe_input_keyboard_event event = { time, keysym, key, !!state, getModifiers(seatData) };
    EventDispatcher::singleton().sendEvent( event );

    if (seatData.keyboard.target.first) {
        struct wpe_view_backend* backend = seatData.keyboard.target.second;
        wpe_view_backend_dispatch_keyboard_event(backend, &event);
    }
}

static gboolean
repeatRateTimeout(void* data)
{
    auto& seatData = *static_cast<Display::SeatData*>(data);
    handleKeyEvent(seatData, seatData.repeatData.key, seatData.repeatData.state, seatData.repeatData.time);
    return G_SOURCE_CONTINUE;
}

static gboolean
repeatDelayTimeout(void* data)
{
    auto& seatData = *static_cast<Display::SeatData*>(data);
    handleKeyEvent(seatData, seatData.repeatData.key, seatData.repeatData.state, seatData.repeatData.time);
    seatData.repeatData.eventSource = g_timeout_add(seatData.repeatInfo.rate, static_cast<GSourceFunc>(repeatRateTimeout), data);
    return G_SOURCE_REMOVE;
}

static const struct wl_keyboard_listener g_keyboardListener = {
    // keymap
    [](void* data, struct wl_keyboard*, uint32_t format, int fd, uint32_t size)
    {
        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
            close(fd);
            return;
        }

        void* mapping = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
        if (mapping == MAP_FAILED) {
            close(fd);
            return;
        }

        auto* xkb = wpe_input_xkb_context_get_default();
        auto* keymap = xkb_keymap_new_from_string(wpe_input_xkb_context_get_context(xkb), static_cast<char*>(mapping),
            XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(mapping, size);
	close(fd);

        wpe_input_xkb_context_set_keymap(xkb, keymap);
        xkb_keymap_unref(keymap);
    },
    // enter
    [](void* data, struct wl_keyboard*, uint32_t serial, struct wl_surface* surface, struct wl_array*)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);
        seatData.serial = serial;
        auto it = seatData.inputClients.find(surface);
        if (it != seatData.inputClients.end())
            seatData.keyboard.target = *it;
    },
    // leave
    [](void* data, struct wl_keyboard*, uint32_t serial, struct wl_surface* surface)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);
        seatData.serial = serial;
        auto it = seatData.inputClients.find(surface);
        if (it != seatData.inputClients.end() && seatData.keyboard.target.first == it->first)
            seatData.keyboard.target = { nullptr, nullptr };
    },
    // key
    [](void* data, struct wl_keyboard*, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
    {
        // IDK.
        key += 8;
        auto& seatData = *static_cast<Display::SeatData*>(data);
        seatData.serial = serial;
        handleKeyEvent(seatData, key, state, time);

        if (!seatData.repeatInfo.rate)
            return;

        auto* keymap = wpe_input_xkb_context_get_keymap(wpe_input_xkb_context_get_default());

        if (state == WL_KEYBOARD_KEY_STATE_RELEASED
            && seatData.repeatData.key == key) {
            if (seatData.repeatData.eventSource)
                g_source_remove(seatData.repeatData.eventSource);
            seatData.repeatData = { 0, 0, 0, 0 };
        } else if (state == WL_KEYBOARD_KEY_STATE_PRESSED
            && keymap && xkb_keymap_key_repeats(keymap, key)) {

            if (seatData.repeatData.eventSource)
                g_source_remove(seatData.repeatData.eventSource);

            seatData.repeatData = { key, time, state, g_timeout_add(seatData.repeatInfo.delay, static_cast<GSourceFunc>(repeatDelayTimeout), data) };
        }
    },
    // modifiers
    [](void* data, struct wl_keyboard*, uint32_t serial, uint32_t depressedMods, uint32_t latchedMods, uint32_t lockedMods, uint32_t group)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);
        seatData.serial = serial;
        seatData.keyboard.modifiers = wpe_input_xkb_context_get_modifiers(wpe_input_xkb_context_get_default(), depressedMods, latchedMods, lockedMods, group);
    },
    // repeat_info
    [](void* data, struct wl_keyboard*, int32_t rate, int32_t delay)
    {
        auto& repeatInfo = static_cast<Display::SeatData*>(data)->repeatInfo;
        repeatInfo = { rate, delay };

        // A rate of zero disables any repeating.
        if (!rate) {
            auto& repeatData = static_cast<Display::SeatData*>(data)->repeatData;
            if (repeatData.eventSource) {
                g_source_remove(repeatData.eventSource);
                repeatData = { 0, 0, 0, 0 };
            }
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

        auto& touchPoints = seatData.touch.touchPoints;
        touchPoints[id] = { wpe_input_touch_event_type_down, time, id, wl_fixed_to_int(x), wl_fixed_to_int(y) };

        auto& target = seatData.touch.targets[id];
        assert(!target.first && !target.second);

        auto it = seatData.inputClients.find(surface);
        if (it == seatData.inputClients.end()) {
            // no surface properly registered, fallback to event_touch_simple
            struct wpe_input_touch_event_raw event_touch_simple = { wpe_input_touch_event_type_down, time, id, wl_fixed_to_int(x), wl_fixed_to_int(y) };
            EventDispatcher::singleton().sendEvent( event_touch_simple );
        } else {
            target = { surface, it->second };
            struct wpe_input_touch_event event = { touchPoints.data(), touchPoints.size(), wpe_input_touch_event_type_down, id, time, getModifiers(seatData) };
            struct wpe_view_backend* backend = target.second;
            wpe_view_backend_dispatch_touch_event(backend, &event);
        }
    },
    // up
    [](void* data, struct wl_touch*, uint32_t serial, uint32_t time, int32_t id)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);
        seatData.serial = serial;

        int32_t arraySize = std::tuple_size<decltype(seatData.touch.targets)>::value;
        if (id < 0 || id >= arraySize)
            return;

        auto& touchPoints = seatData.touch.touchPoints;
        auto& point = touchPoints[id];
        auto& target = seatData.touch.targets[id];

        if (target.first && target.second) {
            point = { wpe_input_touch_event_type_up, time, id, point.x, point.y };
            struct wpe_input_touch_event event = { touchPoints.data(), touchPoints.size(), wpe_input_touch_event_type_up, id, time, getModifiers(seatData) };

            struct wpe_view_backend* backend = target.second;
            wpe_view_backend_dispatch_touch_event(backend, &event);

            point = { wpe_input_touch_event_type_null, 0, 0, 0, 0 };
            target = { nullptr, nullptr };
        } else {
            // no surface registered
            struct wpe_input_touch_event_raw event_touch_simple = { wpe_input_touch_event_type_up, time, id, point.x, point.y };
            EventDispatcher::singleton().sendEvent( event_touch_simple );
        }
    },
    // motion
    [](void* data, struct wl_touch*, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);

        int32_t arraySize = std::tuple_size<decltype(seatData.touch.targets)>::value;
        if (id < 0 || id >= arraySize)
            return;

        auto& touchPoints = seatData.touch.touchPoints;
        touchPoints[id] = { wpe_input_touch_event_type_motion, time, id, wl_fixed_to_int(x), wl_fixed_to_int(y) };
        auto& target = seatData.touch.targets[id];

        if (target.first && target.second) {
            struct wpe_input_touch_event event = { touchPoints.data(), touchPoints.size(), wpe_input_touch_event_type_motion, id, time, getModifiers(seatData) };
            struct wpe_view_backend* backend = target.second;
            wpe_view_backend_dispatch_touch_event(backend, &event);
        } else {
            // no surface registered
            struct wpe_input_touch_event_raw event_touch_simple = { wpe_input_touch_event_type_motion, time, id, wl_fixed_to_int(x), wl_fixed_to_int(y) };
            EventDispatcher::singleton().sendEvent( event_touch_simple );
        }
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

static const struct wl_seat_listener g_seatListener = {
    // capabilities
    [](void* data, struct wl_seat* seat, uint32_t capabilities)
    {
        auto& seatData = *static_cast<Display::SeatData*>(data);

        // WL_SEAT_CAPABILITY_POINTER
        const bool hasPointerCap = capabilities & WL_SEAT_CAPABILITY_POINTER;
        if (hasPointerCap && !seatData.pointer.object) {
            seatData.pointer.object = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(seatData.pointer.object, &g_pointerListener, &seatData);
        }
        if (!hasPointerCap && seatData.pointer.object) {
            wl_pointer_destroy(seatData.pointer.object);
            seatData.pointer.object = nullptr;
        }

        // WL_SEAT_CAPABILITY_KEYBOARD
        const bool hasKeyboardCap = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
        if (hasKeyboardCap && !seatData.keyboard.object) {
            seatData.keyboard.object = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(seatData.keyboard.object, &g_keyboardListener, &seatData);
        }
        if (!hasKeyboardCap && seatData.keyboard.object) {
            wl_keyboard_destroy(seatData.keyboard.object);
            seatData.keyboard.object = nullptr;
        }

        // WL_SEAT_CAPABILITY_TOUCH
        const bool hasTouchCap = capabilities & WL_SEAT_CAPABILITY_TOUCH;
        if (hasTouchCap && !seatData.touch.object) {
            seatData.touch.object = wl_seat_get_touch(seat);
            wl_touch_add_listener(seatData.touch.object, &g_touchListener, &seatData);
        }
        if (!hasTouchCap && seatData.touch.object) {
            wl_touch_destroy(seatData.touch.object);
            seatData.touch.object = nullptr;
        }
    },
    // name
    [](void*, struct wl_seat*, const char*) { }
};


Display& Display::singleton()
{
    static Display display;
    return display;
}

Display::Display()
{
    m_display = wl_display_connect(nullptr);

    if (!m_display) {
        fprintf(stderr, "Wayland::Display: failed to connect\n");
        abort();
    }

    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &g_registryListener, &m_interfaces);
    wl_display_roundtrip(m_display);
    m_eventSource = g_source_new(&EventSource::sourceFuncs, sizeof(EventSource));
    auto* source = reinterpret_cast<EventSource*>(m_eventSource);
    source->display = m_display;
    source->isReading = false;

    source->pfd.fd = wl_display_get_fd(m_display);
    source->pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
    source->pfd.revents = 0;
    g_source_add_poll(m_eventSource, &source->pfd);
    g_source_set_name(m_eventSource, "[WPE] Display");
    g_source_set_priority(m_eventSource, G_PRIORITY_HIGH + 30);
    g_source_set_can_recurse(m_eventSource, TRUE);
    g_source_attach(m_eventSource, g_main_context_get_thread_default());
    if (m_interfaces.xdg) {
        xdg_shell_add_listener(m_interfaces.xdg, &g_xdgShellListener, nullptr);
        xdg_shell_use_unstable_version(m_interfaces.xdg, 5);
    }

    if ( m_interfaces.seat )
        wl_seat_add_listener(m_interfaces.seat, &g_seatListener, &m_seatData);
}

Display::~Display()
{
    if (m_eventSource)
        g_source_unref(m_eventSource);
    m_eventSource = nullptr;

    if (m_interfaces.compositor)
        wl_compositor_destroy(m_interfaces.compositor);
    if (m_interfaces.seat)
        wl_seat_destroy(m_interfaces.seat);
#ifdef BACKEND_BCM_NEXUS_WAYLAND
    if (m_interfaces.nsc)
        wl_nsc_destroy(m_interfaces.nsc);
#endif
    if (m_interfaces.xdg)
        xdg_shell_destroy(m_interfaces.xdg);
    if (m_interfaces.shell)
        wl_shell_destroy(m_interfaces.shell);
    m_interfaces = {
        nullptr,
#ifdef BACKEND_BCM_NEXUS_WAYLAND
        nullptr,
#endif
        nullptr,
        nullptr,
        nullptr,
    };

    if (m_registry)
        wl_registry_destroy(m_registry);
    m_registry = nullptr;
    if (m_display)
        wl_display_disconnect(m_display);
    m_display = nullptr;

    if (m_seatData.pointer.object)
        wl_pointer_destroy(m_seatData.pointer.object);
    if (m_seatData.keyboard.object)
        wl_keyboard_destroy(m_seatData.keyboard.object);
    if (m_seatData.touch.object)
        wl_touch_destroy(m_seatData.touch.object);
    if (m_seatData.repeatData.eventSource)
        g_source_remove(m_seatData.repeatData.eventSource);
    m_seatData = SeatData{ };
}

void Display::registerInputClient(struct wl_surface* surface, struct wpe_view_backend* client)
{
#ifndef NDEBUG
    auto result =
#endif
        m_seatData.inputClients.insert({ surface, client });
    assert(result.second);
}

void Display::unregisterInputClient(struct wl_surface* surface)
{
    auto it = m_seatData.inputClients.find(surface);
    assert(it != m_seatData.inputClients.end());

    if (m_seatData.pointer.target.first == it->first)
        m_seatData.pointer.target = { nullptr, nullptr };
    if (m_seatData.keyboard.target.first == it->first)
        m_seatData.keyboard.target = { nullptr, nullptr };
    m_seatData.inputClients.erase(it);
}


EventDispatcher& EventDispatcher::singleton()
{
    static EventDispatcher event;
    return event;
}

void EventDispatcher::sendEvent( wpe_input_axis_event& event )
{
    if ( m_ipc != nullptr )
    {
        IPC::Message message;
        message.messageCode = MsgType::AXIS;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void EventDispatcher::sendEvent( wpe_input_pointer_event& event )
{
    if ( m_ipc != nullptr )
    {
        IPC::Message message;
        message.messageCode = MsgType::POINTER;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void EventDispatcher::sendEvent( wpe_input_touch_event& event )
{
    if ( m_ipc != nullptr )
    {
        IPC::Message message;
        message.messageCode = MsgType::TOUCH;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void EventDispatcher::sendEvent( wpe_input_keyboard_event& event )
{
    if ( m_ipc != nullptr )
    {
        IPC::Message message;
        message.messageCode = MsgType::KEYBOARD;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void EventDispatcher::sendEvent( wpe_input_touch_event_raw& event )
{
    if ( m_ipc != nullptr )
    {
        IPC::Message message;
        message.messageCode = MsgType::TOUCHSIMPLE;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void EventDispatcher::setIPC( IPC::Client& ipcClient )
{
    m_ipc = &ipcClient;
}

} // namespace Wayland
