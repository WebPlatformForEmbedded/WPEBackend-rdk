# - Try to find libinput.
# Once done, this will define
#
#  LIBINPUT_FOUND - system has libinput.
#  LIBINPUT_INCLUDE_DIRS - the libinput include directories
#  LIBINPUT_LIBRARIES - link these to use libinput.
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

if(Libinput_FIND_QUIETLY)
    set(_LIBINPUT_MODE QUIET)
elseif(Libinput_FIND_REQUIRED)
    set(_LIBINPUT_MODE REQUIRED)
endif()

find_package(PkgConfig)
pkg_check_modules(LIBINPUT ${_LIBINPUT_MODE} libinput)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libinput DEFAULT_MSG LIBINPUT_FOUND)
mark_as_advanced(LIBINPUT_INCLUDE_DIRS LIBINPUT_LIBRARIES)

if(Libinput_FOUND AND NOT TARGET Libinput::Libinput)
    add_library(Libinput::Libinput UNKNOWN IMPORTED)
    set_target_properties(Libinput::Libinput PROPERTIES
            IMPORTED_LOCATION "${LIBINPUT_LIBRARY}"
            INTERFACE_LINK_LIBRARIES "${LIBINPUT_LIBRARIES}"
            INTERFACE_COMPILE_OPTIONS "${LIBINPUT_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBINPUT_INCLUDE_DIRS}"
            )
endif()
