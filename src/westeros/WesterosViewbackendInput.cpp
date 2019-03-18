#include "WesterosViewbackendInput.h"

#include <cstring>
#include <cassert>
#include <cstring>
#include <linux/input.h>
#include <locale.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wpe/wpe.h>
#include <memory>

namespace Westeros {

static WstKeyboardNestedListener keyboard_listener = {
    WesterosViewbackendInput::keyboardHandleKeyMap,
    WesterosViewbackendInput::keyboardHandleEnter,
    WesterosViewbackendInput::keyboardHandleLeave,
    WesterosViewbackendInput::keyboardHandleKey,
    WesterosViewbackendInput::keyboardHandleModifiers,
    WesterosViewbackendInput::keyboardHandleRepeatInfo
};

static WstPointerNestedListener pointer_listener = {
    WesterosViewbackendInput::pointerHandleEnter,
    WesterosViewbackendInput::pointerHandleLeave,
    WesterosViewbackendInput::pointerHandleMotion,
    WesterosViewbackendInput::pointerHandleButton,
    WesterosViewbackendInput::pointerHandleAxis
};

void WesterosViewbackendInput::keyboardHandleKeyMap( void *userData, uint32_t format, int fd, uint32_t size )
{
    auto& backend_input = *static_cast<WesterosViewbackendInput*>(userData);
    auto& handlerData = backend_input.m_handlerData;

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
}

void WesterosViewbackendInput::keyboardHandleEnter( void *userData, struct wl_array *keys )
{
}

void WesterosViewbackendInput::keyboardHandleLeave( void *userData )
{
}

void WesterosViewbackendInput::keyboardHandleKey( void *userData, uint32_t time, uint32_t key, uint32_t state )
{
    auto& backend_input = *static_cast<WesterosViewbackendInput*>(userData);
    auto& handlerData = backend_input.m_handlerData;

    // IDK.
    key += 8;

    handleKeyEvent(userData, key, state, time);

    if (!handlerData.repeatInfo.rate)
        return;

    auto* keymap = wpe_input_xkb_context_get_keymap(wpe_input_xkb_context_get_default());

    if (state == WL_KEYBOARD_KEY_STATE_RELEASED
        && handlerData.repeatData.key == key) {
        if (handlerData.repeatData.eventSource)
            g_source_remove(handlerData.repeatData.eventSource);
        handlerData.repeatData = { 0, 0, 0, 0 };
    } else if (state == WL_KEYBOARD_KEY_STATE_PRESSED
        && keymap && xkb_keymap_key_repeats(keymap, key)) {

        if (handlerData.repeatData.eventSource)
            g_source_remove(handlerData.repeatData.eventSource);

        handlerData.repeatData = { key, time, state, g_timeout_add(handlerData.repeatInfo.delay, static_cast<GSourceFunc>(repeatDelayTimeout), userData) };
    }
}

void WesterosViewbackendInput::keyboardHandleModifiers( void *userData, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group )
{
    auto& backend_input = *static_cast<WesterosViewbackendInput*>(userData);
    auto& handlerData = backend_input.m_handlerData;

    handlerData.modifiers = wpe_input_xkb_context_get_modifiers(wpe_input_xkb_context_get_default(), mods_depressed, mods_latched, mods_locked, group);
}

void WesterosViewbackendInput::keyboardHandleRepeatInfo( void *userData, int32_t rate, int32_t delay )
{
    auto& backend_input = *static_cast<WesterosViewbackendInput*>(userData);
    auto& handlerData = backend_input.m_handlerData;

    auto& repeatInfo = handlerData.repeatInfo;
    repeatInfo = { rate, delay };

    // A rate of zero disables any repeating.
    if (!rate) {
        auto& repeatData = handlerData.repeatData;
        if (repeatData.eventSource) {
            g_source_remove(repeatData.eventSource);
            repeatData = { 0, 0, 0, 0 };
        }
    }
}

struct KeyEventData
{
    void* userData;
    uint32_t key;
    uint32_t state;
    uint32_t time;
    uint32_t modifiers;
};

void WesterosViewbackendInput::handleKeyEvent(void* userData, uint32_t key, uint32_t state, uint32_t time)
{
    auto& me = *static_cast<WesterosViewbackendInput*>(userData);
    if (!me.m_viewbackend)
        return;

    KeyEventData *eventData = new KeyEventData { userData, key, state, time, me.m_handlerData.modifiers };

    std::unique_lock<std::mutex> lock(me.m_evtMutex);
    g_ptr_array_add(me.m_keyEventDataArray, eventData);
    lock.unlock();

    g_idle_add_full(G_PRIORITY_DEFAULT, [](gpointer data) -> gboolean
    {
        KeyEventData *e = (KeyEventData*)data;

        auto& backend_input = *static_cast<WesterosViewbackendInput*>(e->userData);
        auto& handlerData = backend_input.m_handlerData;

        std::unique_ptr<KeyEventData> event_data(e);

        std::unique_lock<std::mutex> lock(backend_input.m_evtMutex);
        g_ptr_array_remove_fast(backend_input.m_keyEventDataArray, data);
        lock.unlock();

        uint32_t keysym = wpe_input_xkb_context_get_key_code(wpe_input_xkb_context_get_default(), e->key, e->state == WL_KEYBOARD_KEY_STATE_PRESSED);
        if (!keysym)
            return G_SOURCE_REMOVE;

        static bool ctrl_override = 0;
        if ((keysym == WPE_KEY_Control_L || keysym == WPE_KEY_Control_R))
        {
            if (e->state == WL_KEYBOARD_KEY_STATE_PRESSED)
                ctrl_override = true;
            else
                ctrl_override = false;
            // IR Mgr sends some input from the remote as Ctrl + 'key' combination.
            // Since some of these combinations are handled below we have to drop individual Ctrl events.
            return G_SOURCE_REMOVE;
        }

        uint8_t modifiers = e->modifiers;
        if ((modifiers == wpe_input_keyboard_modifier_control) || (ctrl_override && modifiers == 0))
        {
            bool should_update_key = true;
            uint32_t hardware_key_code = 0;

            if (keysym == WPE_KEY_l || keysym == WPE_KEY_L) {
                keysym = WPE_KEY_BackSpace;
                hardware_key_code = KEY_BACKSPACE;
            }
            else if (keysym == WPE_KEY_f || keysym == WPE_KEY_F) {
                keysym = WPE_KEY_AudioForward;
                hardware_key_code = KEY_FASTFORWARD;
            }
            else if (keysym == WPE_KEY_w || keysym == WPE_KEY_W) {
                keysym = WPE_KEY_AudioRewind;
                hardware_key_code = KEY_REWIND;
            }
            else if (keysym == WPE_KEY_p || keysym == WPE_KEY_P) {
                keysym = WPE_KEY_AudioPlay;
                hardware_key_code = KEY_PLAYPAUSE;
            }
            else if (keysym == WPE_KEY_0) {
                keysym = WPE_KEY_Red;
                hardware_key_code = KEY_RED;
            }
            else if (keysym == WPE_KEY_1) {
                keysym = WPE_KEY_Green;
                hardware_key_code = KEY_GREEN;
            }
            else if (keysym == WPE_KEY_2) {
                keysym = WPE_KEY_Yellow;
                hardware_key_code = KEY_YELLOW;
            }
            else if (keysym == WPE_KEY_3) {
                keysym = WPE_KEY_Blue;
                hardware_key_code = KEY_BLUE;
            }
            else {
                should_update_key = false;
            }

            if (should_update_key) {
                e->key = hardware_key_code + 8;
                modifiers = 0;
            }
        }

        struct wpe_input_keyboard_event event
                { e->time, keysym, e->key, !!e->state, modifiers };
        wpe_view_backend_dispatch_keyboard_event(backend_input.m_viewbackend, &event);
        return G_SOURCE_REMOVE;
    }, eventData, nullptr);
}

gboolean WesterosViewbackendInput::repeatRateTimeout(void* userData)
{
    auto& backend_input = *static_cast<WesterosViewbackendInput*>(userData);
    auto& handlerData = backend_input.m_handlerData;

    handleKeyEvent(userData, handlerData.repeatData.key, handlerData.repeatData.state, handlerData.repeatData.time);
    return G_SOURCE_CONTINUE;
}

gboolean WesterosViewbackendInput::repeatDelayTimeout(void* userData)
{
    auto& backend_input = *static_cast<WesterosViewbackendInput*>(userData);
    auto& handlerData = backend_input.m_handlerData;

    handleKeyEvent(userData, handlerData.repeatData.key, handlerData.repeatData.state, handlerData.repeatData.time);
    handlerData.repeatData.eventSource = g_timeout_add(1000 / handlerData.repeatInfo.rate, static_cast<GSourceFunc>(repeatRateTimeout), userData);
    return G_SOURCE_REMOVE;
}

void WesterosViewbackendInput::pointerHandleEnter( void *userData, wl_fixed_t sx, wl_fixed_t sy )
{
}

void WesterosViewbackendInput::pointerHandleLeave( void *userData )
{
}

struct MotionEventData
{
    void* userData;
    uint32_t time;
    wl_fixed_t sx;
    wl_fixed_t sy;
};

void WesterosViewbackendInput::pointerHandleMotion( void *userData, uint32_t time, wl_fixed_t sx, wl_fixed_t sy )
{
    auto& me = *static_cast<WesterosViewbackendInput*>(userData);
    if (!me.m_viewbackend)
        return;

    MotionEventData *eventData = new MotionEventData { userData, time, sx, sy };

    std::unique_lock<std::mutex> lock(me.m_evtMutex);
    g_ptr_array_add(me.m_motionEventDataArray, eventData);
    lock.unlock();

    g_idle_add_full(G_PRIORITY_DEFAULT, [](gpointer data) -> gboolean
    {
        MotionEventData *e = (MotionEventData*)data;

        auto& backend_input = *static_cast<WesterosViewbackendInput*>(e->userData);
        auto& handlerData = backend_input.m_handlerData;

        auto x = wl_fixed_to_int(e->sx);
        auto y = wl_fixed_to_int(e->sy);

        auto& pointer = handlerData.pointer;
        pointer.coords = { x, y };

        struct wpe_input_pointer_event event
                { wpe_input_pointer_event_type_motion, e->time, x, y, 0, 0 };
        wpe_view_backend_dispatch_pointer_event(backend_input.m_viewbackend, &event);

        std::unique_lock<std::mutex> lock(backend_input.m_evtMutex);
        g_ptr_array_remove_fast(backend_input.m_motionEventDataArray, data);
        lock.unlock();
        delete e;
        return G_SOURCE_REMOVE;
    }, eventData, nullptr);
}

struct ButtonEventData
{
    void* userData;
    uint32_t time;
    uint32_t button;
    uint32_t state;
};

void WesterosViewbackendInput::pointerHandleButton( void *userData, uint32_t time, uint32_t button, uint32_t state )
{
    auto& me = *static_cast<WesterosViewbackendInput*>(userData);
    if (!me.m_viewbackend)
        return;

    button = (button >= BTN_MOUSE) ? (button - BTN_MOUSE + 1) : 0;
    ButtonEventData *eventData = new ButtonEventData { userData, time, button, state };

    std::unique_lock<std::mutex> lock(me.m_evtMutex);
    g_ptr_array_add(me.m_buttonEventDataArray, eventData);
    lock.unlock();

    g_idle_add_full(G_PRIORITY_DEFAULT, [](gpointer data) -> gboolean
    {
        ButtonEventData *e = (ButtonEventData*)data;

        auto& backend_input = *static_cast<WesterosViewbackendInput*>(e->userData);
        auto& handlerData = backend_input.m_handlerData;

        auto& pointer = handlerData.pointer;
        auto& coords = pointer.coords;

        struct wpe_input_pointer_event event
                { wpe_input_pointer_event_type_button, e->time, coords.first, coords.second, e->button, e->state };
        wpe_view_backend_dispatch_pointer_event(backend_input.m_viewbackend, &event);

        std::unique_lock<std::mutex> lock(backend_input.m_evtMutex);
        g_ptr_array_remove_fast(backend_input.m_buttonEventDataArray, data);
        lock.unlock();
        delete e;
        return G_SOURCE_REMOVE;
    }, eventData, nullptr);
}

struct AxisEventData
{
    void* userData;
    uint32_t time;
    uint32_t axis;
    wl_fixed_t value;
};

void WesterosViewbackendInput::pointerHandleAxis( void *userData, uint32_t time, uint32_t axis, wl_fixed_t value )
{
    auto& me = *static_cast<WesterosViewbackendInput*>(userData);
    if (!me.m_viewbackend)
        return;

    AxisEventData *eventData = new AxisEventData { userData, time, axis, value };
    std::unique_lock<std::mutex> lock(me.m_evtMutex);
    g_ptr_array_add(me.m_axisEventDataArray, eventData);
    lock.unlock();

    g_idle_add_full(G_PRIORITY_DEFAULT, [](gpointer data) -> gboolean
    {
        AxisEventData *e = (AxisEventData*)data;

        auto& backend_input = *static_cast<WesterosViewbackendInput*>(e->userData);
        auto& handlerData = backend_input.m_handlerData;

        auto& pointer = handlerData.pointer;
        auto& coords = pointer.coords;

        struct wpe_input_axis_event event{ wpe_input_axis_event_type_motion, e->time, coords.first, coords.second, e->axis, -wl_fixed_to_int(e->value) };
        wpe_view_backend_dispatch_axis_event(backend_input.m_viewbackend, &event);

        std::unique_lock<std::mutex> lock(backend_input.m_evtMutex);
        g_ptr_array_remove_fast(backend_input.m_axisEventDataArray, data);
        lock.unlock();
        delete e;
        return G_SOURCE_REMOVE;
    }, eventData, nullptr);
}

WesterosViewbackendInput::WesterosViewbackendInput(struct wpe_view_backend* backend)
 : m_compositor(nullptr)
 , m_viewbackend(backend)
 , m_handlerData()
 , m_keyEventDataArray(g_ptr_array_sized_new(4))
 , m_motionEventDataArray(g_ptr_array_sized_new(4))
 , m_buttonEventDataArray(g_ptr_array_sized_new(4))
 , m_axisEventDataArray(g_ptr_array_sized_new(4))
{
}

WesterosViewbackendInput::~WesterosViewbackendInput()
{
    m_compositor = nullptr;
    m_viewbackend = nullptr;

    clearDataArrays();

    if (m_handlerData.repeatData.eventSource)
        g_source_remove(m_handlerData.repeatData.eventSource);
}

void WesterosViewbackendInput::initializeNestedInputHandler(WstCompositor *compositor)
{
    m_compositor = compositor;

    if (m_compositor && m_viewbackend)
    {
        if (!WstCompositorSetKeyboardNestedListener( m_compositor, &keyboard_listener, this )) {
            fprintf(stderr, "ViewBackendWesteros: failed to set keyboard nested listener: %s\n",
                WstCompositorGetLastErrorDetail(m_compositor));
        }
        if (!WstCompositorSetPointerNestedListener( m_compositor, &pointer_listener, this )) {
            fprintf(stderr, "WesterosViewbackendInput: failed to set pointer nested listener: %s\n",
                WstCompositorGetLastErrorDetail(m_compositor));
        }
    }
}

template<class Data>
void clearArray(GPtrArray *array)
{
    if (array->len)
    {
        g_ptr_array_foreach(array, [](gpointer data, gpointer user_data)
        {
            g_idle_remove_by_data(data);
            delete (Data*)data;
        }, nullptr);
    }
    g_ptr_array_free(array, TRUE);
}

template<class Data>
void clearArraySafe(GPtrArray *array)
{
    g_idle_add_full(G_PRIORITY_HIGH, [](gpointer data) -> gboolean
    {
        clearArray<Data>((GPtrArray*)data);
        return G_SOURCE_REMOVE;
    }, array, nullptr);
}

void WesterosViewbackendInput::clearDataArrays()
{
    if (g_main_context_is_owner(g_main_context_default()))
    {
        clearArray<KeyEventData>(m_keyEventDataArray);
        clearArray<MotionEventData>(m_motionEventDataArray);
        clearArray<ButtonEventData>(m_buttonEventDataArray);
        clearArray<AxisEventData>(m_axisEventDataArray);
    }
    else
    {
        clearArraySafe<KeyEventData>(m_keyEventDataArray);
        clearArraySafe<MotionEventData>(m_motionEventDataArray);
        clearArraySafe<ButtonEventData>(m_buttonEventDataArray);
        clearArraySafe<AxisEventData>(m_axisEventDataArray);
    }
}

} // namespace Westeros
