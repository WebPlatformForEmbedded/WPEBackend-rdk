/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <wpe/wpe-egl.h>

#include <string.h>
#include <stdlib.h>

#include <linux/input.h>
#include <essos-app.h>
#include <essos-system.h>

#include "ipc.h"
#include "ipc-essos.h"

#define ERROR_LOG(fmt, ...) fprintf(stderr, "[essos:renderer-backend.cpp:%u:%s] *** " fmt "\n", __LINE__, __func__, ##__VA_ARGS__)
#define WARN_LOG(fmt, ...)  fprintf(stderr, "[essos:renderer-backend.cpp:%u:%s] Warning: " fmt "\n", __LINE__, __func__, ##__VA_ARGS__)
#define DEBUG_LOG(fmt, ...) if (enableDebugLogs()) fprintf(stderr, "[essos:renderer-backend.cpp:%u:%s] " fmt "\n", __LINE__, __func__, ##__VA_ARGS__)

extern "C" {
struct wl_display;
int wl_display_flush (struct wl_display *display);
}

namespace Essos {

static bool enableDebugLogs()
{
    static bool enable = !!getenv("WPE_ESSOS_DEBUG");
    return enable;
}

struct Backend
{
    Backend();
    ~Backend();

    NativeDisplayType getDisplay() const;

    EssCtx *essosCtx { nullptr };
};


Backend::Backend()
{
    bool error = false;

    essosCtx = EssContextCreate();

    if ( !EssContextInit(essosCtx) ) {
        error = true;
    }

    DEBUG_LOG("Essos ctx = %p", essosCtx);

    if ( error ) {
        const char *detail = EssContextGetLastErrorDetail(essosCtx);
        ERROR_LOG("Essos error: '%s'", detail);
        if ( essosCtx ) {
            EssContextDestroy(essosCtx);
            essosCtx = nullptr;
            abort();
        }
    }
}

Backend::~Backend()
{
    if (essosCtx)
        EssContextDestroy(essosCtx);
}

NativeDisplayType Backend::getDisplay() const
{
    NativeDisplayType displayType = 0;

    if (!essosCtx || !EssContextGetEGLDisplayType(essosCtx, &displayType))
        return 0;

    DEBUG_LOG("displayType=%x ", displayType);
    return displayType;
}

struct EGLTarget : public IPC::Client::Handler
{
    EGLTarget(struct wpe_renderer_backend_egl_target*, int);
    virtual ~EGLTarget();

    void initialize(Backend& backend, uint32_t width, uint32_t height);
    EGLNativeWindowType getNativeWindow() const;
    void resize(uint32_t width, uint32_t height);
    void frameRendered();

    gboolean runEventLoopOnce();
    void stop();

    // IPC::Client::Handler
    void handleMessage(char* data, size_t size) override;

    // Essos event listeners
    bool updateKeyModifiers(unsigned int key, bool pressed);
    bool updateButtonModifiers(unsigned int button, bool pressed);
    void onDisplaySize(int width, int height);
    void onKeyEvent(unsigned int key, bool pressed);
    void onKeyRepeat(unsigned int key);
    void onPointerMotion(int x, int y);
    void onPointerButton(int button, int x, int y, bool pressed);
    void onTouchDown(int id, int x, int y);
    void onTouchUp(int id);
    void onTouchMoution(int id, int x, int y);
    void onTerminated();

    uint32_t getSourceTimeMs() const { return g_source_get_time(eventSource) / 1000; }

    static EssSettingsListener settingsListener;
    static EssKeyListener keyListener;
    static EssPointerListener pointerListener;
    static EssTouchListener touchListener;
    static EssTerminateListener terminateListener;

    IPC::Client ipcClient;

    struct wpe_renderer_backend_egl_target *target { nullptr };
    Backend *backend { nullptr };
    EssCtx *essosCtx { nullptr };
    GSource *eventSource { nullptr };

    NativeWindowType nativeWindow { 0 };
    int pageWidth { 0 };
    int pageHeight { 0 };
    uint32_t inputModifiers { 0 };
    uint32_t shouldDispatchFrameComplete { 0 };

    xkb_mod_mask_t modShiftMask;
    xkb_mod_mask_t modAltMask;
    xkb_mod_mask_t modCtrlMask;
    xkb_mod_mask_t modMetaMask;
};

EssSettingsListener EGLTarget::settingsListener = {
    // displaySize
    [](void *data, int width, int height ) { reinterpret_cast<EGLTarget*>(data)->onDisplaySize(width, height); },
    // displaySafeArea
    nullptr
};

EssKeyListener EGLTarget::keyListener = {
    // keyPressed
    [](void* data, unsigned int key) { reinterpret_cast<EGLTarget*>(data)->onKeyEvent(key, true); },
    // keyReleased
    [](void* data, unsigned int key) { reinterpret_cast<EGLTarget*>(data)->onKeyEvent(key, false); },
    // keyRepeat
    [](void* data, unsigned int key) { reinterpret_cast<EGLTarget*>(data)->onKeyRepeat(key); },
};

EssPointerListener EGLTarget::pointerListener = {
    // pointerMotion
    []( void *data, int x, int y ) { reinterpret_cast<EGLTarget*>(data)->onPointerMotion(x, y); },
    // pointerButtonPressed
    []( void *data, int button, int x, int y ) { reinterpret_cast<EGLTarget*>(data)->onPointerButton(button, x, y, true); },
    // pointerButtonReleased
    []( void *data, int button, int x, int y ) { reinterpret_cast<EGLTarget*>(data)->onPointerButton(button, x, y, false); },
};

EssTouchListener EGLTarget::touchListener = {
    // touchDown
    []( void *data, int id, int x, int y ) { reinterpret_cast<EGLTarget*>(data)->onTouchDown(id, x, y); },
    // touchUp
    []( void *data, int id ) { reinterpret_cast<EGLTarget*>(data)->onTouchUp(id); },
    // touchMotion
    []( void *data, int id, int x, int y ) { reinterpret_cast<EGLTarget*>(data)->onTouchMoution(id, x, y); },
    // touchFrame
    nullptr
};

EssTerminateListener EGLTarget::terminateListener = {
    // terminated
    []( void *data ) { reinterpret_cast<EGLTarget*>(data)->onTerminated(); },
};

EGLTarget::EGLTarget(struct wpe_renderer_backend_egl_target* target, int hostFd)
    : target(target)
{
    ipcClient.initialize(*this, hostFd);
}

EGLTarget::~EGLTarget()
{
    stop();
}

void EGLTarget::stop()
{
    ipcClient.deinitialize();

    if (essosCtx) {
        EssContextSetSettingsListener(essosCtx, nullptr, nullptr);
        EssContextSetKeyListener(essosCtx, nullptr, nullptr);
        EssContextSetPointerListener(essosCtx, nullptr, nullptr);
        EssContextSetTouchListener(essosCtx, nullptr, nullptr);
        EssContextSetTerminateListener(essosCtx, nullptr, nullptr);

        if (nativeWindow != 0) {
            if ( !EssContextDestroyNativeWindow(essosCtx, nativeWindow) ) {
                const char *detail = EssContextGetLastErrorDetail(essosCtx);
                ERROR_LOG("Essos error: '%s'", detail);
            }
        }

        EssContextStop(essosCtx);
        essosCtx = nullptr;
    }

    if (eventSource) {
        auto *tmp = std::exchange(eventSource, nullptr);
        g_source_destroy(tmp);
        g_source_unref(tmp);
    }

    backend = nullptr;
}

void EGLTarget::initialize(Backend& backend, uint32_t width, uint32_t height)
{
    if (this->backend)
        return;

    if (backend.essosCtx == nullptr) {
        ERROR_LOG("No Essos context");
        return;
    }

    this->backend = &backend;
    essosCtx = backend.essosCtx;
    pageWidth = width;
    pageHeight = height;

    DEBUG_LOG("initial page size=%ux%u", width, height);
    static int fps = []() -> int {
        int result = -1;
        const char *env = getenv("WPE_ESSOS_CYCLES_PER_SECOND");
        if (env)
            result = atoi(env);
        return result < 0 ? 60 : result;
    }();
    eventSource = g_timeout_source_new(1000 / fps);
    g_source_set_callback(
        eventSource,
        [](gpointer data) -> gboolean {
            EGLTarget& self = *reinterpret_cast<EGLTarget*>(data);
            return self.runEventLoopOnce();
        },
        this,
        [](gpointer data) {
            EGLTarget& self = *reinterpret_cast<EGLTarget*>(data);
            if (self.eventSource)
                self.stop();
        });
    g_source_set_priority(eventSource, G_PRIORITY_HIGH + 30);
    g_source_set_can_recurse(eventSource, TRUE);
    g_source_attach(eventSource, g_main_context_get_thread_default());

    bool error = false;
    int targetWidth = pageWidth, targetHeight = pageHeight;

    if ( !EssContextGetUseDirect(essosCtx) ) {
        // Run fullscreen in non direct mode
        if ( !EssContextGetDisplaySize(essosCtx, &targetWidth, &targetHeight ) ) {
            error = true;
        }
        else if ( !EssContextSetSettingsListener(essosCtx, this, &settingsListener) ) {
            error = true;
        }
    }

    DEBUG_LOG("initial target size=%dx%d", targetWidth, targetHeight);

    if ( error || !EssContextSetKeyListener(essosCtx, this, &keyListener) ) {
        error = true;
    }
    else if ( !EssContextSetPointerListener(essosCtx, this, &pointerListener) ) {
        error = true;
    }
    else if ( !EssContextSetTouchListener(essosCtx, this, &touchListener) ) {
        error = true;
    }
    else if ( !EssContextSetTerminateListener(essosCtx, this, &terminateListener) ) {
        error = true;
    }
    else if ( !EssContextCreateNativeWindow(essosCtx, targetWidth, targetHeight, &nativeWindow) ) {
        error = true;
    }
    else if ( !EssContextStart(essosCtx) ) {
        error = true;
    }
    else {
        // Request page resize if needed
        onDisplaySize(targetWidth, targetHeight);
    }

    if ( error ) {
        const char *detail = EssContextGetLastErrorDetail(essosCtx);
        ERROR_LOG("Essos error: '%s'", detail);
        stop();
        abort();
        return;
    }

    auto *wpexkb = wpe_input_xkb_context_get_default();
    auto *keymap = wpe_input_xkb_context_get_keymap(wpexkb);
    modShiftMask = ( 1 << xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_SHIFT) );
    modAltMask   = ( 1 << xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_ALT) );
    modCtrlMask  = ( 1 << xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CTRL) );
    modMetaMask  = ( 1 << xkb_keymap_mod_get_index(keymap, "Meta") );

    // Suppress annoying messages about missing compose file
    auto *compose_table = wpe_input_xkb_context_get_compose_table(wpexkb);
    if (!compose_table) {
        compose_table = xkb_compose_table_new_from_buffer(
            wpe_input_xkb_context_get_context(wpexkb),
            nullptr, 0,
            setlocale(LC_CTYPE, NULL),
            XKB_COMPOSE_FORMAT_TEXT_V1,
            XKB_COMPOSE_COMPILE_NO_FLAGS);
        if (compose_table) {
            wpe_input_xkb_context_set_compose_table(wpexkb, compose_table);
            xkb_compose_table_unref(compose_table);
        }
    }
}

gboolean EGLTarget::runEventLoopOnce()
{
    if (essosCtx)
        EssContextRunEventLoopOnce( essosCtx );

    if ( shouldDispatchFrameComplete ) {
        --shouldDispatchFrameComplete;
        wpe_renderer_backend_egl_target_dispatch_frame_complete( target );
    }
    return G_SOURCE_CONTINUE;
}

void EGLTarget::handleMessage(char* data, size_t size)
{
    if (size != IPC::Message::size)
        return;

    auto& message = IPC::Message::cast(data);
    ERROR_LOG("EGLTarget: unhandled message (%d)", message.messageCode);
}

void EGLTarget::resize(uint32_t width, uint32_t height)
{
    DEBUG_LOG("got new page size=%ux%u", width, height);

    pageWidth = width;
    pageHeight = height;

    EssContextResizeWindow(essosCtx, width, height);
}

void EGLTarget::frameRendered()
{
    if (essosCtx == nullptr)
       return;

    IPC::Message message;
    message.messageCode = IPC::Essos::MsgType::FRAMERENDERED;
    ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);

    ++shouldDispatchFrameComplete;

    if ( EssContextGetUseWayland(essosCtx) ) {
        void* display = EssContextGetWaylandDisplay( essosCtx );
        wl_display_flush( (wl_display*) display );
    }
}

bool EGLTarget::updateKeyModifiers(unsigned int key, bool pressed)
{
    bool isModifierKey = false;
    wpe_input_modifier modifier;

    switch (key)
    {
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            modifier = wpe_input_keyboard_modifier_control;
            isModifierKey = true;
            break;
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            modifier = wpe_input_keyboard_modifier_shift;
            isModifierKey = true;
            break;
        case KEY_LEFTALT:
        case KEY_RIGHTALT:
           modifier = wpe_input_keyboard_modifier_alt;
           isModifierKey = true;
           break;
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA:
            modifier = wpe_input_keyboard_modifier_meta;
            isModifierKey = true;
            break;
        default:
            break;
    }

    if (isModifierKey)
    {
        if (pressed)
            inputModifiers |= modifier;
        else
            inputModifiers &= ~modifier;
    }

    return isModifierKey;
}

void EGLTarget::onKeyEvent(unsigned int key, bool pressed)
{
    if (updateKeyModifiers(key, pressed))
        return;

    uint32_t activeModifiers = inputModifiers;
    if (activeModifiers == wpe_input_keyboard_modifier_control) {  // only Ctrl is set
        switch(key) {
            case KEY_L: key = KEY_BACKSPACE;   activeModifiers = 0; break;
            case KEY_F: key = KEY_FASTFORWARD; activeModifiers = 0; break;
            case KEY_W: key = KEY_REWIND;      activeModifiers = 0; break;
            case KEY_P: key = KEY_PLAYPAUSE;   activeModifiers = 0; break;
            case KEY_0: key = KEY_RED;         activeModifiers = 0; break;
            case KEY_1: key = KEY_GREEN;       activeModifiers = 0; break;
            case KEY_2: key = KEY_YELLOW;      activeModifiers = 0; break;
            case KEY_3: key = KEY_BLUE;        activeModifiers = 0; break;
            default: break;
        }
    }

    // Offset between key codes defined in /usr/include/linux/input.h and /usr/share/X11/xkb/keycodes/evdev
    key += 8;

    xkb_mod_mask_t depresedMask = 0;
    if (activeModifiers & wpe_input_keyboard_modifier_control)
        depresedMask |= modCtrlMask;
    if (activeModifiers & wpe_input_keyboard_modifier_shift)
        depresedMask |= modShiftMask;
    if (activeModifiers & wpe_input_keyboard_modifier_alt)
        depresedMask |= modAltMask;
    if (activeModifiers & wpe_input_keyboard_modifier_meta)
        depresedMask |= modMetaMask;

    uint32_t time = getSourceTimeMs();
    uint32_t modifiers = wpe_input_xkb_context_get_modifiers(wpe_input_xkb_context_get_default(), depresedMask, 0, 0, 0);
    uint32_t keysym = wpe_input_xkb_context_get_key_code(wpe_input_xkb_context_get_default(), key, pressed);

    DEBUG_LOG("hw key=%u, xkb keysym=%u, modifiers=%u (%s)", key, keysym, modifiers, pressed ? "pressed" : "released" );

    struct wpe_input_keyboard_event event =
        {
            time, keysym, key, pressed, modifiers
        };
    IPC::Message message;
    message.messageCode = IPC::Essos::MsgType::KEYBOARD;
    memcpy(message.messageData, &event, sizeof(event));
    ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void EGLTarget::onKeyRepeat(unsigned int key)
{
    onKeyEvent(key, true);
}

void EGLTarget::onPointerMotion(int x, int y)
{
    uint32_t time = getSourceTimeMs();
    struct wpe_input_pointer_event event =
        {
            wpe_input_pointer_event_type_motion,
            time, x, y, 0, 0, inputModifiers
        };
    IPC::Message message;
    message.messageCode = IPC::Essos::MsgType::POINTER;
    memcpy(message.messageData, &event, sizeof(event));
    ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

bool EGLTarget::updateButtonModifiers(unsigned int button_id, bool pressed)
{
    wpe_input_modifier modifier;
    switch (button_id)
    {
        case 1:
            modifier = wpe_input_pointer_modifier_button1;
            break;
        case 2:
            modifier = wpe_input_pointer_modifier_button2;
            break;
        case 3:
            modifier = wpe_input_pointer_modifier_button3;
            break;
        default:
            return false;
    }

    if (pressed)
        inputModifiers |= modifier;
    else
        inputModifiers &= ~modifier;

    return true;
}

void EGLTarget::onPointerButton(int button, int x, int y, bool pressed)
{
    uint32_t time = getSourceTimeMs();
    uint32_t button_idx = (button >= BTN_MOUSE) ? (button - BTN_MOUSE + 1) : 0;

    updateButtonModifiers(button_idx, pressed);

    struct wpe_input_pointer_event event =
        {
            wpe_input_pointer_event_type_button,
            time, x, y, button_idx, pressed, inputModifiers
        };

    IPC::Message message;
    message.messageCode = IPC::Essos::MsgType::POINTER;
    memcpy(message.messageData, &event, sizeof(event));
    ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void EGLTarget::onTouchDown(int id, int x, int y)
{
    uint32_t time = getSourceTimeMs();
    struct wpe_input_touch_event_raw event =
        {
            wpe_input_touch_event_type_down,
            time, id, x, y
        };
    IPC::Message message;
    message.messageCode = IPC::Essos::MsgType::TOUCHSIMPLE;
    memcpy(message.messageData, &event, sizeof(event));
    ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void EGLTarget::onTouchUp(int id)
{
    uint32_t time = getSourceTimeMs();
    struct wpe_input_touch_event_raw event =
        {
            wpe_input_touch_event_type_up,
            time, id, 0, 0
        };
    IPC::Message message;
    message.messageCode = IPC::Essos::MsgType::TOUCHSIMPLE;
    memcpy(message.messageData, &event, sizeof(event));
    ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void EGLTarget::onTouchMoution(int id, int x, int y)
{
    uint32_t time = getSourceTimeMs();
    struct wpe_input_touch_event_raw event =
        {
            wpe_input_touch_event_type_motion,
            time, id, x, y
        };
    IPC::Message message;
    message.messageCode = IPC::Essos::MsgType::TOUCHSIMPLE;
    memcpy(message.messageData, &event, sizeof(event));
    ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void EGLTarget::onDisplaySize(int width, int height)
{
    if (pageWidth == width && pageHeight == height)
        return;
    DEBUG_LOG("display size=%dx%d, page size=%dx%d", width, height, pageWidth, pageHeight);
    IPC::Message message;
    IPC::Essos::DisplaySize::construct(message, width, height);
    ipcClient.sendMessage(IPC::Message::data(message), IPC::Message::size);
}

void EGLTarget::onTerminated()
{
    WARN_LOG("Terminated. Essos ctx = %p", essosCtx);
    stop();
}

EGLNativeWindowType EGLTarget::getNativeWindow() const
{
    return nativeWindow;
}

} // namespace Essos

extern "C" {

struct wpe_renderer_backend_egl_interface essos_renderer_backend_egl_interface = {
    // create
    [](int) -> void*
    {
        return new Essos::Backend;
    },
    // destroy
    [](void* data)
    {
        auto* backend = static_cast<Essos::Backend*>(data);
        delete backend;
    },
    // get_native_display
    [](void* data) -> EGLNativeDisplayType
    {
        auto& backend = *static_cast<Essos::Backend*>(data);
        return backend.getDisplay();
    },
};

struct wpe_renderer_backend_egl_target_interface essos_renderer_backend_egl_target_interface = {
    // create
    [](struct wpe_renderer_backend_egl_target* target, int host_fd) -> void*
    {
        return new Essos::EGLTarget(target, host_fd);
    },
    // destroy
    [](void* data)
    {
        auto* target = static_cast<Essos::EGLTarget*>(data);
        delete target;
    },
    // initialize
    [](void* data, void* backend_data, uint32_t width, uint32_t height)
    {
        auto& target = *static_cast<Essos::EGLTarget*>(data);
        auto& backend = *static_cast<Essos::Backend*>(backend_data);
        target.initialize(backend, width, height);
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType
    {
        auto& target = *static_cast<Essos::EGLTarget*>(data);
        return target.getNativeWindow();
    },
    // resize
    [](void* data, uint32_t width, uint32_t height)
    {
        auto& target = *static_cast<Essos::EGLTarget*>(data);
        target.resize(width, height);
    },
    // frame_will_render
    [](void* data)
    {
    },
    // frame_rendered
    [](void* data)
    {
        auto& target = *static_cast<Essos::EGLTarget*>(data);
        target.frameRendered();
    },
};

struct wpe_renderer_backend_egl_offscreen_target_interface essos_renderer_backend_egl_offscreen_target_interface = {
    // create
    []() -> void*
    {
        return nullptr;
    },
    // destroy
    [](void* data)
    {
    },
    // initialize
    [](void* data, void* backend_data)
    {
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType
    {
        return (EGLNativeWindowType)nullptr;
    },
};

}
