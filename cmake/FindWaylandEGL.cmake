# - Try to find wayland-egl.
# Once done, this will define
#
#  WAYLAND_EGL_INCLUDE_DIRS - the wayland-egl include directories
#  WAYLAND_EGL_LIBRARIES - link these to use wayland-egl.
#  WAYLAND_EGL_CFLAGS - Platform specific cflags to use.
#
# Copyright (C) 2015 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS
# IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

find_package(PkgConfig)
pkg_check_modules(PC_WAYLAND_EGL wayland-egl)

set(WAYLAND_EGL_FOUND ${PC_WAYLAND_EGL_FOUND} CACHE INTERNAL "" FORCE)

find_path(WAYLAND_EGL_INCLUDE_DIRS
    NAMES wayland-egl.h
    HINTS ${PC_WAYLAND_EGL_INCLUDE_DIRS} ${PC_WAYLAND_EGL_INCLUDEDIR}
) 

foreach (_library ${PC_WAYLAND_EGL_LIBRARIES})
    find_library(WAYLAND_EGL_LIBRARIES_${_library} ${_library}
        HINTS ${PC_WAYLAND_EGL_LIBRARY_DIRS} ${PC_WAYLAND_EGL_LIBDIR}
    )
    set(WAYLAND_EGL_LIBRARIES ${WAYLAND_EGL_LIBRARIES} ${WAYLAND_EGL_LIBRARIES_${_library}})
endforeach ()

set(WAYLAND_EGL_CFLAGS ${PC_WAYLAND_EGL_CFLAGS})

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(WAYLAND_EGL DEFAULT_MSG WAYLAND_EGL_LIBRARIES)

mark_as_advanced(WAYLAND_EGL_INCLUDE_DIRS WAYLAND_EGL_LIBRARIES)
