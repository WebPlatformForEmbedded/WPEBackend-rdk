find_package(EGL REQUIRED)

add_definitions(-DBACKEND_VIV_IMX6_EGL=1)

#Following is necessary for eglvivante.h 
add_definitions(-DLINUX -DEGL_API_FB)

list(APPEND WPE_PLATFORM_INCLUDE_DIRECTORIES
    ${EGL_INCLUDE_DIRS}
)

list(APPEND WPE_PLATFORM_LIBRARIES
    ${EGL_LIBRARIES}
)

list(APPEND WPE_PLATFORM_SOURCES
    src/viv-imx6/renderer-backend.cpp
    src/viv-imx6/view-backend.cpp
)
