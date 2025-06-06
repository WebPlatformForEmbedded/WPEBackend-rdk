find_package(Westeros REQUIRED)
find_package(EGL REQUIRED)
find_package(WaylandEGL REQUIRED)

add_definitions(-DBACKEND_WESTEROS=1)

list(APPEND WPE_PLATFORM_INCLUDE_DIRECTORIES
    ${WAYLAND_EGL_INCLUDE_DIRS}
)

list(APPEND WPE_PLATFORM_LIBRARIES
    ${WAYLAND_EGL_LIBRARIES}
    Westeros::Westeros
    EGL::EGL
)

list(APPEND WPE_PLATFORM_SOURCES
    src/westeros/renderer-backend.cpp
    src/westeros/view-backend.cpp
    src/westeros/WesterosViewbackendInput.cpp
    src/westeros/WesterosViewbackendOutput.cpp
)
