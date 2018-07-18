find_package(Wayland QUIET)
find_package(CompositorClient QUIET)

if (COMPOSITOR_CLIENT_FOUND)

    list(APPEND WPE_PLATFORM_INCLUDE_DIRECTORIES
        ${COMPOSITOR_CLIENT_INCLUDE_DIRS}
    )

    list(APPEND WPE_PLATFORM_LIBRARIES
        ${COMPOSITOR_CLIENT_LIBRARIES}
    )

    list(APPEND WPE_PLATFORM_EXTRA_CFLAGS
        ${COMPOSITOR_CLIENT_CFLAGS}
    )
endif()

if (WAYLAND_FOUND)
    find_package(WaylandEGL REQUIRED)

    list(APPEND WPE_PLATFORM_INCLUDE_DIRECTORIES
        ${WAYLAND_INCLUDE_DIRS}
        ${WAYLAND_EGL_INCLUDE_DIRS}
    )

    list(APPEND WPE_PLATFORM_LIBRARIES
        ${WAYLAND_LIBRARIES}
        ${WAYLAND_EGL_LIBRARIES}
    )

    list(APPEND WPE_PLATFORM_EXTRA_CFLAGS
        ${WAYLAND_EGL_CFLAGS}
    )

endif()

add_definitions(-DBACKEND_WPEFRAMEWORK=1)
add_definitions(-DEGL_EGLEXT_PROTOTYPES=1)

list(APPEND WPE_PLATFORM_SOURCES
    src/wpeframework/renderer-backend.cpp
    src/wpeframework/view-backend.cpp
    src/wpeframework/display.cpp
)


