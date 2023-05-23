# - Try to find Wayland
# Once done this will define
#  WAYLAND_FOUND - System has Nexus
#  WAYLAND_INCLUDE_DIRS - The Nexus include directories
#  WAYLAND_LIBRARIES    - The libraries needed to use Nexus
#
#  Wayland::Wayland, the wayland server library
#
# Copyright (C) 2014 Igalia S.L.
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

if(Wayland_FIND_QUIETLY)
    set(_WAYLAND_SERVER_MODE QUIET)
elseif(Wayland_FIND_REQUIRED)
    set(_WAYLAND_SERVER_MODE REQUIRED)
endif()

find_package(PkgConfig)
pkg_check_modules(WAYLAND ${_WAYLAND_SERVER_MODE} wayland-client>=1.2 wayland-server)

find_library(WAYLAND_LIB NAMES wayland-client
        HINTS ${WAYLAND_LIBDIR} ${WAYLAND_LIBRARY_DIRS})

# If Wayland includes are located in standard path i.e. "/usr/include" get this path manually to make next functions happy
if(WAYLAND_INCLUDE_DIRS STREQUAL "")
    pkg_get_variable(WAYLAND_INCLUDE_DIRS wayland-client includedir)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Wayland DEFAULT_MSG WAYLAND_FOUND WAYLAND_INCLUDE_DIRS WAYLAND_LIBRARIES WAYLAND_LIB)
mark_as_advanced(WAYLAND_INCLUDE_DIRS WAYLAND_LIBRARIES)

if(Wayland_FOUND AND NOT TARGET Wayland::Wayland)
    add_library(Wayland::Wayland UNKNOWN IMPORTED)
    set_target_properties(Wayland::Wayland PROPERTIES
            IMPORTED_LOCATION "${WAYLAND_LIB}"
            INTERFACE_LINK_LIBRARIES "${WAYLAND_LIBRARIES}"
            INTERFACE_COMPILE_OPTIONS "${WAYLAND_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${WAYLAND_INCLUDE_DIRS}"
            )
endif()
