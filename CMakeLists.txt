cmake_minimum_required(VERSION 2.8)
project(WPEBackend-rdk)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -fno-exceptions -fno-strict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -fno-exceptions -fno-strict-aliasing -fno-rtti")

# Converted
option(USE_BACKEND_BCM_NEXUS "Whether to enable support for the BCM_NEXUS WPE backend" OFF)
option(USE_BACKEND_BCM_NEXUS_WAYLAND "Whether to enable support for the BCM_NEXUS Wayland WPE backend" OFF)
option(USE_BACKEND_BCM_RPI "Whether to enable support for the BCM_RPi WPE backend" OFF)
option(USE_BACKEND_INTEL_CE "Whether to enable support for the Intel CE WPE backend" OFF)
option(USE_BACKEND_WAYLAND_EGL "Whether to enable support for the wayland-egl WPE backend" OFF)
option(USE_BACKEND_WESTEROS "Whether to enable support for the Westeros WPE backend" OFF)
option(USE_BACKEND_REALTEK "Whether to enable support for Realtek's Wayland EGL WPE backend" OFF)
option(USE_BACKEND_VIV_IMX6_EGL "Whether to enable support for NXP's IMX6 EGL WPE backend" OFF)
option(USE_BACKEND_WPEFRAMEWORK "WPEFramework abstraction layer is used as WPE backend" OFF)

option(USE_PLATFORM_BROADCOM "Whether the playback is based on Broadcom plugins" OFF)
option(USE_BACKEND_WESTEROS_MESA "Whether to enable support for the gbm based offscreen target for westeros Mesa only" OFF)

option(USE_INPUT_LIBINPUT "Whether to enable support for the libinput input backend" ON)
option(USE_INPUT_UDEV "Whether to enable support for the libinput input udev lib" ON)
option(USE_INPUT_WAYLAND "Whether to enable support for the wayland input backend" OFF)
option(USE_GENERIC_GAMEPAD "Use generic gamepad implementation" OFF)

find_package(WPE REQUIRED)
find_package(EGL REQUIRED)
find_package(Libxkbcommon REQUIRED)
find_package(GLIB 2.38.0 REQUIRED COMPONENTS gio gio-unix)

if (USE_BACKEND_WESTEROS_MESA)
    add_definitions(-DWPE_BACKEND_MESA=1)
endif ()

set(WPE_PLATFORM_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/src/input"
        "${CMAKE_SOURCE_DIR}/src/util"
        ${GIO_UNIX_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
        ${WPE_INCLUDE_DIRS}
        )

set(WPE_PLATFORM_LIBRARIES
        ${GLIB_GIO_LIBRARIES}
        ${GLIB_GOBJECT_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${WPE_LIBRARIES}
        ${LIBXKBCOMMON_LIBRARIES}
        )

if (USE_VIRTUAL_KEYBOARD)
    find_package(WPEFrameworkVirtualInput REQUIRED)
    list(APPEND WPE_PLATFORM_LIBRARIES
        WPEFrameworkVirtualInput::WPEFrameworkVirtualInput
    )
    add_definitions(-DKEY_INPUT_HANDLING_VIRTUAL=1)
endif()

set(WPE_PLATFORM_EXTRA_CFLAGS)

set(WPE_PLATFORM_SOURCES
        src/loader-impl.cpp

        src/util/ipc.cpp
        )

if (EGL_FOUND)
    add_definitions(${PC_EGL_CFLAGS})
endif ()

if (USE_INPUT_LIBINPUT)
    find_package(Libinput REQUIRED)
    list(APPEND WPE_PLATFORM_INCLUDE_DIRECTORIES
            ${LIBINPUT_INCLUDE_DIRS}
            )
    list(APPEND WPE_PLATFORM_LIBRARIES
            ${LIBINPUT_LIBRARIES}
            )
    if (USE_INPUT_UDEV)
        find_package(Libudev REQUIRED)
        list(APPEND WPE_PLATFORM_INCLUDE_DIRECTORIES
                ${LIBUDEV_INCLUDE_DIRS}
                )
        list(APPEND WPE_PLATFORM_LIBRARIES
                ${LIBUDEV_LIBRARIES}
                )
        add_definitions(-DKEY_INPUT_UDEV=1)
    endif ()
    list(APPEND WPE_PLATFORM_SOURCES
            src/input/Libinput/KeyboardEventRepeating.cpp
            src/input/Libinput/LibinputServer.cpp
            )
elseif (USE_VIRTUAL_KEYBOARD)
    list(APPEND WPE_PLATFORM_SOURCES
            src/input/Libinput/KeyboardEventRepeating.cpp
            src/input/Libinput/LibinputServer.cpp
            )
endif()

if (USE_GENERIC_GAMEPAD)
    include(src/gamepad/CMakeLists.txt)
    include(src/gamepad_test/CMakeLists.txt)
endif()

if (USE_BACKEND_BCM_NEXUS)
    include(src/bcm-nexus/CMakeLists.txt)
endif ()

if (USE_BACKEND_BCM_NEXUS_WAYLAND)
    include(src/bcm-nexus-wayland/CMakeLists.txt)
endif ()

if (USE_BACKEND_BCM_RPI)
    include(src/bcm-rpi/CMakeLists.txt)
endif ()

if (USE_BACKEND_INTEL_CE)
    include(src/intelce/CMakeLists.txt)
endif ()

if (USE_BACKEND_REALTEK)
    include(src/realtek-wl-egl/CMakeLists.txt)
endif ()

if (USE_BACKEND_WPEFRAMEWORK)
    include(src/wpeframework/CMakeLists.txt)
endif ()

if (USE_BACKEND_WAYLAND_EGL)
    include(src/wayland-egl/CMakeLists.txt)
endif ()

if (USE_BACKEND_WESTEROS)
    include(src/westeros/CMakeLists.txt)
endif ()

if (USE_BACKEND_VIV_IMX6_EGL)
    include(src/viv-imx6/CMakeLists.txt)
endif ()

add_library(WPEBackend-rdk SHARED ${WPE_PLATFORM_SOURCES})
target_include_directories(WPEBackend-rdk PRIVATE ${WPE_PLATFORM_INCLUDE_DIRECTORIES})
target_link_libraries(WPEBackend-rdk ${WPE_PLATFORM_LIBRARIES})
target_compile_options(WPEBackend-rdk PRIVATE ${WPE_PLATFORM_EXTRA_CFLAGS})
install(TARGETS WPEBackend-rdk DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")

# Create a libWPEBackend-default.so symlink to libWPEBackend-rdk.so and install it.
add_custom_command(TARGET WPEBackend-rdk
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink libWPEBackend-rdk.so ${CMAKE_BINARY_DIR}/libWPEBackend-default.so
        )
install(FILES ${CMAKE_BINARY_DIR}/libWPEBackend-default.so DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
