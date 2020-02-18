LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/src/directfb/cmake")

find_package(EGL REQUIRED)
find_package(DirectFB REQUIRED)

add_definitions(-DBACKEND_DIRECTFB=1)

list(APPEND WPE_PLATFORM_INCLUDE_DIRECTORIES
        ${DIRECTFB_INCLUDE_DIRS}
        ${RTEUTILS_INCLUDE_DIRS}
        "${CMAKE_SOURCE_DIR}/src/diretfb/"
)

list(APPEND WPE_PLATFORM_LIBRARIES
    ${RTEUTILS_LIBRARIES}
    ${EGL_LIBRARIES_dbpl}
)

list(APPEND WPE_PLATFORM_SOURCES
    src/directfb/renderer-backend.cpp
    src/directfb/view-backend.cpp
    src/directfb/display.cpp
)

if (USE_INPUT_LIBINPUT OR USE_VIRTUAL_KEYBOARD)
    add_definitions(-DKEY_INPUT_HANDLING_LIBINPUT=1)
endif ()


#add_definitions(-DPLATFORM_BRCM=1) # Special DFB code handling for broadcom platforms
