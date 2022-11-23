# - Try to find libudev.
# Once done, this will define
#
#  LIBUDEV_FOUND - system has udev.
#  LIBUDEV_INCLUDE_DIRS - the udev include directories
#  LIBUDEV_LIBRARIES - link these to use udev.
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

if(Libudev_FIND_QUIETLY)
    set(_LIBUDEV_MODE QUIET)
elseif(Libudev_FIND_REQUIRED)
    set(_LIBUDEV_MODE REQUIRED)
endif()

find_package(PkgConfig)
pkg_check_modules(PC_LIBUDEV ${_LIBUDEV_MODE} libudev)

find_path(LIBUDEV_INCLUDE_DIRS
    NAMES libudev.h
    HINTS ${PC_LIBUDEV_INCLUDEDIR} ${PC_LIBUDEV_INCLUDE_DIRS}
)

find_library(LIBUDEV_LIBRARIES
    NAMES udev
    HINTS ${PC_LIBUDEV_LIBDIR} ${PC_LIBUDEV_LIBRARY_DIRS}
)

mark_as_advanced(LIBUDEV_INCLUDE_DIRS LIBUDEV_LIBRARIES)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Libudev
    REQUIRED_VARS LIBUDEV_INCLUDE_DIRS LIBUDEV_LIBRARIES
    FOUND_VAR LIBUDEV_FOUND
    VERSION_VAR PC_LIBUDEV_VERSION)

if(Libudev_FOUND AND NOT TARGET Libudev::Libudev)
    add_library(Libudev::Libudev UNKNOWN IMPORTED)
    set_target_properties(Libudev::Libudev PROPERTIES
            IMPORTED_LOCATION "${LIBUDEV_LIBRARY}"
            INTERFACE_LINK_LIBRARIES "${LIBUDEV_LIBRARIES}"
            INTERFACE_COMPILE_OPTIONS "${LIBUDEV_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBUDEV_INCLUDE_DIRS}"
            )
endif()
