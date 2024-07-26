find_package(Wayland QUIET)
find_package(WaylandEGL QUIET)
find_package(EGL REQUIRED)
find_package(ClientCompositor REQUIRED)

message(STATUS "Building with compositor client")
list(APPEND WPE_PLATFORM_LIBRARIES
    ClientCompositor::ClientCompositor
)

if (WAYLAND_FOUND AND WAYLAND_EGL_FOUND AND WPE_VERSION VERSION_GREATER 1.4)
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

    list(APPEND WPE_PLATFORM_SOURCES
        src/thunder/compositorclient-get-surface.cpp
    )

    set(WPEBACKEND_RDK_PUBLIC_HEADERS
        src/thunder/compositorclient-get-surface.h
    )

    install(
        FILES ${WPEBACKEND_RDK_PUBLIC_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wpe-rdk-${WPEBACKEND_RDK_API_VERSION}/wpe
    )
endif()

add_definitions(-DBACKEND_THUNDER=1)
add_definitions(-DEGL_EGLEXT_PROTOTYPES=1)

list(APPEND WPE_PLATFORM_SOURCES
    src/thunder/renderer-backend.cpp
    src/thunder/view-backend.cpp
    src/thunder/display.cpp
)
