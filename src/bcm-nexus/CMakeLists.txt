find_package(EGL REQUIRED)

add_definitions(-DBACKEND_BCM_NEXUS=1)

list(APPEND WPE_PLATFORM_LIBRARIES
    nxpl
    v3ddriver
)

if (EGL_LIBRARIES_nxclient)
    message("-- Found NXCLIENT: TRUE [egl.pc]")
    add_definitions(-DBACKEND_BCM_NEXUS_NXCLIENT=1 ${EGL_DEFINITIONS})
    list(APPEND WPE_PLATFORM_LIBRARIES nxclient)
endif()

# TODO: write a pc file for this.
if (NEXUS_CLIENT_MODE)
    message(STATUS "Building with nexus in client mode" )
    list(APPEND WPE_PLATFORM_LIBRARIES nexus_client)
else ()
    message(STATUS "Building with nexus in proxy mode" )
    list(APPEND WPE_PLATFORM_LIBRARIES nexus)
endif ()

list(APPEND WPE_PLATFORM_SOURCES
    src/bcm-nexus/renderer-backend.cpp
    src/bcm-nexus/view-backend.cpp
)

if (USE_INPUT_LIBINPUT OR USE_VIRTUAL_KEYBOARD)
    add_definitions(-DKEY_INPUT_HANDLING_LIBINPUT=1)
endif ()

if (USE_INPUT_WAYLAND)
    add_definitions(-DKEY_INPUT_HANDLING_WAYLAND=1)

    find_package(Wayland REQUIRED)
    list(APPEND WPE_PLATFORM_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/src/wayland"
        "${CMAKE_SOURCE_DIR}/src/wayland/protocols"
        ${WAYLAND_INCLUDE_DIRS}
    )

    list(APPEND WPE_PLATFORM_LIBRARIES
        ${WAYLAND_LIBRARIES}
    )

    list(APPEND WPE_PLATFORM_SOURCES
        src/wayland/protocols/xdg-shell-protocol.c
        src/wayland/display.cpp
    )
endif ()
