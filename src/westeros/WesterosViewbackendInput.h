#ifndef WPE_ViewBackend_WesterosViewbackendInput_h
#define WPE_ViewBackend_WesterosViewbackendInput_h

#include <glib.h>
#include <utility>
#include <mutex>
#include <wayland-client.h>
#include <westeros-compositor.h>

struct wpe_view_backend;

namespace Westeros {

class WesterosViewbackendInput {
public:
    WesterosViewbackendInput(struct wpe_view_backend*);
    virtual ~WesterosViewbackendInput();
    void initializeNestedInputHandler(WstCompositor *compositor);
    void deinitialize() { m_viewbackend = nullptr; }

    static void keyboardHandleKeyMap( void *userData, uint32_t format, int fd, uint32_t size );
    static void keyboardHandleEnter( void *userData, struct wl_array *keys );
    static void keyboardHandleLeave( void *userData );
    static void keyboardHandleKey( void *userData, uint32_t time, uint32_t key, uint32_t state );
    static void keyboardHandleModifiers( void *userData, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group );
    static void keyboardHandleRepeatInfo( void *userData, int32_t rate, int32_t delay );
    static void pointerHandleEnter( void *userData, wl_fixed_t sx, wl_fixed_t sy );
    static void pointerHandleLeave( void *userData );
    static void pointerHandleMotion( void *userData, uint32_t time, wl_fixed_t sx, wl_fixed_t sy );
    static void pointerHandleButton( void *userData, uint32_t time, uint32_t button, uint32_t state );
    static void pointerHandleAxis( void *userData, uint32_t time, uint32_t axis, wl_fixed_t value );
    static void handleKeyEvent(void* userData, uint32_t key, uint32_t state, uint32_t time);
    static gboolean repeatRateTimeout(void* userData);
    static gboolean repeatDelayTimeout(void* userData);

    struct HandlerData {
        struct {
            std::pair<int, int> coords;
        } pointer { { 0, 0 } };

        uint32_t modifiers { 0 };

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

private:
    void clearDataArrays();

    std::mutex m_evtMutex;
    WstCompositor* m_compositor;
    struct wpe_view_backend* m_viewbackend;
    HandlerData m_handlerData;
    GPtrArray* m_keyEventDataArray;
    GPtrArray* m_motionEventDataArray;
    GPtrArray* m_buttonEventDataArray;
    GPtrArray* m_axisEventDataArray;
    GMainContext *m_mainContext;
};

} // namespace Westeros

#endif // WPE_ViewBackend_WesterosViewbackendInput_h
