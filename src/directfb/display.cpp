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

#include "display.h"

#include <cassert>
#include <cstring>
#include <glib.h>
#include <linux/input.h>
#include <locale.h>
#include <memory>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wpe/wpe.h>

namespace Directfb {

    class EventSource {
    public:
        static GSourceFuncs sourceFuncs;

        GSource source;
        GPollFD pfd;
        IDirectFBEventBuffer* eventBuffer;
    };

static uint32_t
get_time_from_timeval( const struct timeval *tv )
{
  long long monotonic = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
  long long realtime  = direct_clock_get_time( DIRECT_CLOCK_REALTIME );

  return tv->tv_sec * 1000 + tv->tv_usec / 1000 + monotonic / 1000 - realtime / 1000;
}

static xkb_keysym_t
dfb_key_xkb_translate(DFBInputDeviceKeyIdentifier key_id,
                    DFBInputDeviceKeySymbol key_symbol)
{
	xkb_keysym_t keysym = XKB_KEY_NoSymbol;

	/* special case numpad */
	if (key_id >= DIKI_KP_DIV && key_id <= DIKI_KP_9) {
		switch (key_symbol) {
			case DIKS_SLASH:         keysym = WPE_KEY_KP_Divide;    break;
			case DIKS_ASTERISK:      keysym = WPE_KEY_KP_Multiply;  break;
			case DIKS_PLUS_SIGN:     keysym = WPE_KEY_KP_Add;       break;
			case DIKS_MINUS_SIGN:    keysym = WPE_KEY_KP_Subtract;  break;
			case DIKS_ENTER:         keysym = WPE_KEY_KP_Enter;     break;
			case DIKS_SPACE:         keysym = WPE_KEY_KP_Space;     break;
			case DIKS_TAB:           keysym = WPE_KEY_KP_Tab;       break;
			case DIKS_EQUALS_SIGN:   keysym = WPE_KEY_KP_Equal;     break;
			case DIKS_COMMA:
			case DIKS_PERIOD:        keysym = WPE_KEY_KP_Decimal;   break;
			case DIKS_HOME:          keysym = WPE_KEY_KP_Home;      break;
			case DIKS_END:           keysym = WPE_KEY_KP_End;       break;
			case DIKS_PAGE_UP:       keysym = WPE_KEY_KP_Page_Up;   break;
			case DIKS_PAGE_DOWN:     keysym = WPE_KEY_KP_Page_Down; break;
			case DIKS_CURSOR_LEFT:   keysym = WPE_KEY_KP_Left;      break;
			case DIKS_CURSOR_RIGHT:  keysym = WPE_KEY_KP_Right;     break;
			case DIKS_CURSOR_UP:     keysym = WPE_KEY_KP_Up;        break;
			case DIKS_CURSOR_DOWN:   keysym = WPE_KEY_KP_Down;      break;
			case DIKS_BEGIN:         keysym = WPE_KEY_KP_Begin;     break;
			case DIKS_INSERT:        keysym = WPE_KEY_KP_Insert;    break;
			case DIKS_DELETE:        keysym = WPE_KEY_KP_Delete;    break;
			case DIKS_0 ... DIKS_9:  keysym = WPE_KEY_0 + key_symbol - DIKS_0; break;
			case DIKS_F1 ... DIKS_F4:keysym = WPE_KEY_F1 + key_symbol - DIKS_F1; break;

			default:
				keysym = WPE_KEY_Escape;
			break;
        }
	} else {
		switch (DFB_KEY_TYPE (key_symbol)) {
			case DIKT_UNICODE:
				switch (key_symbol)	{
					case DIKS_NULL:       keysym = WPE_KEY_VoidSymbol; break;
					case DIKS_BACKSPACE:  keysym = WPE_KEY_BackSpace;  break;
					case DIKS_TAB:        keysym = WPE_KEY_Tab;        break;
					case DIKS_RETURN:     keysym = WPE_KEY_Return;     break; /* same as DIKS_ENTER*/
					case DIKS_CANCEL:     keysym = WPE_KEY_Cancel;     break;
					case DIKS_ESCAPE:     keysym = WPE_KEY_Escape;     break;
					case DIKS_SPACE:      keysym = WPE_KEY_space;      break;
					case DIKS_DELETE:     keysym = WPE_KEY_Delete;     break;
					case DIKS_BACK:       keysym = WPE_KEY_Back;       break;

					default:
						if (keysym & 0x01000000)
							keysym = WPE_KEY_VoidSymbol;
						else
							keysym = key_symbol;
				}
				break;

			case DIKT_SPECIAL:
				switch (key_symbol) {
					case DIKS_CURSOR_LEFT:   keysym = WPE_KEY_Left;      break;
					case DIKS_CURSOR_RIGHT:  keysym = WPE_KEY_Right;     break;
					case DIKS_CURSOR_UP:     keysym = WPE_KEY_Up;        break;
					case DIKS_CURSOR_DOWN:   keysym = WPE_KEY_Down;      break;
					case DIKS_INSERT:        keysym = WPE_KEY_Insert;    break;
					case DIKS_HOME:          keysym = WPE_KEY_Home;      break;
					case DIKS_END:           keysym = WPE_KEY_End;       break;
					case DIKS_PAGE_UP:       keysym = WPE_KEY_Page_Up;   break;
					case DIKS_PAGE_DOWN:     keysym = WPE_KEY_Page_Down; break;
					case DIKS_PRINT:         keysym = WPE_KEY_Print;     break;
					case DIKS_PAUSE:         keysym = WPE_KEY_Pause;     break;
					case DIKS_SELECT:        keysym = WPE_KEY_Select;    break;
					case DIKS_CLEAR:         keysym = WPE_KEY_Clear;     break;
					case DIKS_MENU:          keysym = WPE_KEY_Menu;      break;
					case DIKS_HELP:          keysym = WPE_KEY_Help;      break;
					case DIKS_BEGIN:         keysym = WPE_KEY_Begin;     break;
					case DIKS_BREAK:         keysym = WPE_KEY_Break;     break;
					case DIKS_OK:            keysym = WPE_KEY_Return;    break;
					case DIKS_BACK:          keysym = WPE_KEY_Back;      break;
					default:
						/* Pass thru all other DIKT_SPECIAL keys */
						keysym = key_symbol;
						break;
				}
				break;

			case DIKT_FUNCTION:
				keysym = WPE_KEY_F1 + key_symbol - DIKS_F1;
				if (keysym > WPE_KEY_F35)
					keysym = WPE_KEY_VoidSymbol;
				break;

			case DIKT_MODIFIER:
				switch (key_id)	{
					case DIKI_SHIFT_L:    keysym = WPE_KEY_Shift_L;     break;
					case DIKI_SHIFT_R:    keysym = WPE_KEY_Shift_R;     break;
					case DIKI_CONTROL_L:  keysym = WPE_KEY_Control_L;   break;
					case DIKI_CONTROL_R:  keysym = WPE_KEY_Control_R;   break;
					case DIKI_ALT_L:      keysym = WPE_KEY_Alt_L;       break;
					case DIKI_ALT_R:      keysym = WPE_KEY_Alt_R;       break;
					case DIKI_META_L:     keysym = WPE_KEY_Meta_L;      break;
					case DIKI_META_R:     keysym = WPE_KEY_Meta_R;      break;
					case DIKI_SUPER_L:    keysym = WPE_KEY_Super_L;     break;
					case DIKI_SUPER_R:    keysym = WPE_KEY_Super_R;     break;
					case DIKI_HYPER_L:    keysym = WPE_KEY_Hyper_L;     break;
					case DIKI_HYPER_R:    keysym = WPE_KEY_Hyper_R;     break;
					default:
					break;
				}
				break;

			case DIKT_LOCK:
				switch (key_symbol) {
					case DIKS_CAPS_LOCK:    keysym = WPE_KEY_Caps_Lock;   break;
					case DIKS_NUM_LOCK:     keysym = WPE_KEY_Num_Lock;    break;
					case DIKS_SCROLL_LOCK:  keysym = WPE_KEY_Scroll_Lock; break;
					default:
						break;
				}
				break;

			case DIKT_DEAD:
				/* dead keys are handled directly by directfb */
				break;

			case DIKT_CUSTOM:
				/* Pass thru all DIKT_CUSTOM keys */
				keysym = key_symbol;
				break;
			} /*switch*/
		} /* else*/

    return keysym;
}

static void
handleKeyEvent(uint32_t  time_,
				int32_t  key_id,
				int32_t  key_code,
				int32_t  key_symbol,
				uint32_t state,
				gpointer data)
{
    auto* seatData = reinterpret_cast<Display::SeatData*>(data);
	xkb_keysym_t keysym = XKB_KEY_NoSymbol;
	uint32_t unicode = key_symbol;

	if( key_code <= 0 ||
        ((keysym = xkb_state_key_get_one_sym (seatData->xkb.state, key_code)) == XKB_KEY_NoSymbol) ||
        keysym >= 0x1008FF00) { /* if system doesnt understand XKB symbols of extended (internet/multimedia) keyboard inputs*/
            keysym = dfb_key_xkb_translate((DFBInputDeviceKeyIdentifier)key_id, (DFBInputDeviceKeySymbol)key_symbol);
    }
    if(key_code > 0) {
        unicode = xkb_state_key_get_utf32(seatData->xkb.state, key_code);
        xkb_state_update_key(seatData->xkb.state, key_code, state ? XKB_KEY_DOWN : XKB_KEY_UP);
    }
    struct wpe_input_keyboard_event event = { time_, keysym, unicode, state ? true : false, seatData->xkb.modifiers };
    EventDispatcher::singleton().sendEvent( event );

    fprintf(stdout, "DIET_%s : key_id 0x%X , key_sym 0x%X, key_code 0x%X xkb_keysym_t 0x%X unicode %u modifiers %u\n",
                                    state == DIET_KEYPRESS ? "KEYPRESS":"KEYRELEASE", key_id, key_symbol, key_code, keysym, unicode, seatData->xkb.modifiers);
    return;
}

static void
handleKeyModifier(const DFBWindowEvent *input_event, gpointer data)
{
    auto* seatData = reinterpret_cast<Display::SeatData*>(data);
    auto& xkb = seatData->xkb;

    uint32_t latched_mods = 0, locked_mods = 0, base_mods = 0;
    uint8_t keyModifiers = 0;

	auto& modifiers = xkb.modifiers;
	modifiers = 0;

    if(input_event->modifiers || input_event->locks) {
        /*XKB modifier Index values shift : 0, caps : 1, ctrl : 2, alt : 3, num : 4*/
        if(input_event->modifiers & DIMM_SHIFT)
            latched_mods |= (1U << 0);
        if(input_event->locks & DILS_CAPS)
            locked_mods |= (1U << 1);
        if(input_event->modifiers & DIMM_CONTROL)
            base_mods |= (1U << 2);
        if(input_event->modifiers & DIMM_ALT)
            base_mods |= (1U << 3);
        if(input_event->locks & DILS_NUM)
            locked_mods |= (1U << 4);
    }
    xkb_state_update_mask (xkb.state, base_mods, latched_mods, locked_mods, 0, 0, 0);

    if(input_event->modifiers || input_event->locks) {
        auto component = static_cast<xkb_state_component>(XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);
        if (xkb_state_mod_index_is_active(xkb.state, xkb.indexes.control, component))
            modifiers |= wpe_input_keyboard_modifier_control;
        if (xkb_state_mod_index_is_active(xkb.state, xkb.indexes.alt, component))
            modifiers |= wpe_input_keyboard_modifier_alt;
        if (xkb_state_mod_index_is_active(xkb.state, xkb.indexes.shift, component))
            modifiers |= wpe_input_keyboard_modifier_shift;
    }
    return;
}

static void
handleDfbEvent(DFBEvent &event, gpointer data)
{
    switch(event.clazz) {
        case DFEC_WINDOW: {
            const DFBWindowEvent  &input = event.window;
            switch(input.type) {
                 case DWET_KEYDOWN:
                 case DWET_KEYUP: {
                    handleKeyModifier(&input, data); /* Not required in final build, we wont use keyboard*/
                    handleKeyEvent(get_time_from_timeval(&input.timestamp),
                                    input.key_id,
                                    (input.key_code > 0) ? (input.key_code + 8) : input.key_code, // Keycode system starts at 8.
                                    input.key_symbol,
                                    (input.type == DWET_KEYDOWN),
                                    data);
                     break;
                 }
                 default: /* Mouse events not handled */
                    break;
            }
        }
        default:
            fprintf(stdout, "!!!!!!!!!! UnHandled Event Class %d  !!!!!!!!!!\n", event.clazz);
            break;
    }
}

GSourceFuncs EventSource::sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        if(source->pfd.fd == -1) {
            source->eventBuffer->Reset( source->eventBuffer );
            return TRUE;
        }
        *timeout = -1;

        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);

        if (source->pfd.revents & G_IO_IN) {
            return TRUE;
        } else {
            return FALSE;
        }
    },
    // dispatch
    [](GSource* base, GSourceFunc callback, gpointer data) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        auto* seatData = reinterpret_cast<Display::SeatData*>(data);
        DFBEvent event;
        unsigned char buffer[sizeof(DFBEvent)];

        if (source->pfd.revents & G_IO_IN) {
            ssize_t bytes;
            size_t bufferOffset = 0;
            assert(bufferOffset < sizeof(DFBEvent));
            const size_t needed = sizeof(DFBEvent) - bufferOffset;
            do {
                bytes = ::read(source->pfd.fd, buffer + bufferOffset, needed);
            } while(bytes == -1 && errno == EINTR);
            assert(bytes != -1 || errno == EWOULDBLOCK || errno == EAGAIN);
            if(bytes > 0) {
                bufferOffset += bytes;
                if(bufferOffset == sizeof(DFBEvent)) {
                    memcpy(&event, buffer, sizeof(DFBEvent));
                    bufferOffset = 0;
                }
            }
            handleDfbEvent(event, data);
        } else if(source->pfd.fd == -1) {
            if(source->eventBuffer->WaitForEvent(source->eventBuffer) != DFB_OK) {
                source->eventBuffer->GetEvent( source->eventBuffer, DFB_EVENT(&event) );
                handleDfbEvent(event, data);
            }
        }

        if (source->pfd.revents & (G_IO_ERR | G_IO_HUP))
            return G_SOURCE_REMOVE;

        source->pfd.revents = 0;
        return G_SOURCE_CONTINUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

void Display::unregisterDirectFBDisplayPlatform()
{
    if (m_dfb) {
        /* Add Platform Specific code */
#if defined(PLATFORM_BRCM)
        if(m_dfbPlatformHandle) {
            DBPL_UnregisterDirectFBDisplayPlatform(m_dfbPlatformHandle);
            m_dfbPlatformHandle = 0;
        }
#endif
    }
}

void Display::deInitDirectFB()
{
    if (m_dfb) {
        m_dfb->Release( m_dfb );
        m_dfb = NULL;
    }
}

bool Display::initDirectFB()
{
    if (m_dfb)
        return true;

    if (DirectFBInit(NULL, NULL) != DFB_OK)
        return false;

    if (DirectFBCreate(&m_dfb) != DFB_OK)
        return false;

    IDirectFBDisplayLayer *layer = NULL;
    m_dfb->GetDisplayLayer( m_dfb, DLID_PRIMARY, &layer );
    assert(layer);
    if(layer){
        layer->SetCooperativeLevel( layer, DLSCL_SHARED );
        layer->GetConfiguration(layer, &m_layerConfig);
        layer->Release(layer);
    }
    return true;
}

bool Display::registerDirectFBDisplayPlatform()
{
    /* Add Platform Specific code */
#if defined(PLATFORM_BRCM)
    if(0 == m_dfbPlatformHandle)
        DBPL_RegisterDirectFBDisplayPlatform(&m_dfbPlatformHandle, m_dfb);
#endif
    return true;
}

Display& Display::singleton()
{
    static Display display;
    return display;
}

Display::Display()
    :m_dfb(NULL)
#if defined(PLATFORM_BRCM)
    ,m_dfbPlatformHandle(0)
#endif
{
    memset(&m_layerConfig, sizeof(m_layerConfig), 0);

    struct xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_rule_names names = { "evdev", "pc105", "us", "", "" };

    initDirectFB();

    m_seatData.xkb.keymap = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!m_seatData.xkb.keymap){
        fprintf(stderr, "Dfb::Display Couldnt create xkbKeymap\n");
        abort();
    }
    m_seatData.xkb.state = xkb_state_new(m_seatData.xkb.keymap);
    if (!m_seatData.xkb.state){
        fprintf(stderr, "Dfb::Display Couldnt create xkbState\n");
        abort();
    }

    m_seatData.xkb.indexes.control = xkb_keymap_mod_get_index(m_seatData.xkb.keymap, XKB_MOD_NAME_CTRL);
    m_seatData.xkb.indexes.alt = xkb_keymap_mod_get_index(m_seatData.xkb.keymap, XKB_MOD_NAME_ALT);
    m_seatData.xkb.indexes.shift = xkb_keymap_mod_get_index(m_seatData.xkb.keymap, XKB_MOD_NAME_SHIFT);
    m_seatData.xkb.modifiers = 0;

    xkb_context_unref(context);
}

void Display::initializeEventSource(IDirectFBWindow *dfb_window)
{
    DFBResult ret;
    IDirectFBEventBuffer *eventBuffer = NULL;

    if(ret = m_dfb->CreateEventBuffer(m_dfb,&eventBuffer)) {
        fprintf(stderr, "xxxxxxxxxx Create Direct FB Event Buffer failed xxxxxxxxxx %s", DirectFBErrorString(ret));
    } else {
        dfb_window->AttachEventBuffer(dfb_window, eventBuffer);
        dfb_window->RequestFocus(dfb_window);
    }
    fprintf(stderr, "Initialize DirectFb Event Input handling\n");

    m_eventSource = g_source_new(&EventSource::sourceFuncs, sizeof(EventSource));
    auto* source = reinterpret_cast<EventSource*>(m_eventSource);

    source->eventBuffer = eventBuffer;

	if(source->eventBuffer) {
        if((ret = source->eventBuffer->CreateFileDescriptor(source->eventBuffer, &source->pfd.fd)) != DFB_OK) {
            fprintf(stderr, "xxxxxxxxxx Couldnt Create FD from eventBuffer xxxxxxxxxx %s", DirectFBErrorString(ret));
            source->pfd.fd = -1;
        }
    }
    source->pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
    source->pfd.revents = 0;
    g_source_add_poll(m_eventSource, &source->pfd);
    g_source_set_name(m_eventSource, "[WPE] DirectFb EventBuffer");
    g_source_set_priority(m_eventSource, G_PRIORITY_HIGH + 30);
    g_source_set_can_recurse(m_eventSource, TRUE);
    g_source_attach(m_eventSource, g_main_context_get_thread_default());
    g_source_set_callback (m_eventSource, static_cast<GSourceFunc>(NULL), &m_seatData, NULL);
    fprintf(stdout, "Initialized with G_SOURCE input handling\n");
    return;
}

Display::~Display()
{
    if (m_eventSource) {
        auto* source = reinterpret_cast<EventSource*>(m_eventSource);
        source->eventBuffer->WakeUp( source->eventBuffer );
        g_source_unref(m_eventSource);
        m_eventSource = nullptr;
    }

    unregisterDirectFBDisplayPlatform(); /* XXX do we need this here or to use atexit */
    deInitDirectFB();

    m_seatData = SeatData{ };

    if (m_seatData.xkb.keymap)
        xkb_keymap_unref(m_seatData.xkb.keymap);
    if (m_seatData.xkb.state)
        xkb_state_unref(m_seatData.xkb.state);
}

EventDispatcher& EventDispatcher::singleton()
{
    static EventDispatcher event;
    return event;
}

void EventDispatcher::sendEvent( wpe_input_axis_event& event )
{
    if ( m_ipc != nullptr ) {
        IPC::Message message;
        message.messageCode = MsgType::AXIS;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void EventDispatcher::sendEvent( wpe_input_pointer_event& event )
{
    if ( m_ipc != nullptr ) {
        IPC::Message message;
        message.messageCode = MsgType::POINTER;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void EventDispatcher::sendEvent( wpe_input_touch_event& event )
{
    if ( m_ipc != nullptr ) {
        IPC::Message message;
        message.messageCode = MsgType::TOUCH;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void EventDispatcher::sendEvent( wpe_input_keyboard_event& event )
{
    if ( m_ipc != nullptr ) {
        IPC::Message message;
        message.messageCode = MsgType::KEYBOARD;
        memcpy( message.messageData, &event, sizeof(event) );
        m_ipc->sendMessage(IPC::Message::data(message), IPC::Message::size);
    }
}

void EventDispatcher::sendEvent( wpe_input_touch_event_raw& event )
{
    if ( m_ipc != nullptr ) {
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
