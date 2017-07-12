find_package(Wayland REQUIRED)
find_package(WaylandEGL REQUIRED)
find_package(CompositorClient REQUIRED)

add_definitions(-DBACKEND_COMPOSITOR_CLIENT=1)
add_definitions(-DEGL_EGLEXT_PROTOTYPES=1)

list(APPEND WPE_PLATFORM_INCLUDE_DIRECTORIES
    ${WAYLAND_INCLUDE_DIRS}
    ${WAYLAND_EGL_INCLUDE_DIRS}
    ${COMPOSITOR_CLIENT_INCLUDE_DIRS}
)

list(APPEND WPE_PLATFORM_LIBRARIES
    ${WAYLAND_LIBRARIES}
    ${WAYLAND_EGL_LIBRARIES}
    ${COMPOSITOR_CLIENT_LIBRARIES}
)

list(APPEND WPE_PLATFORM_SOURCES
    src/compositor-client/renderer-backend.cpp
    src/compositor-client/view-backend.cpp
)
