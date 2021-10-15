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

if(RTKlibs_FIND_QUIETLY)
    set(_REALTEK_MODE QUIET)
else(RTKlibs_FIND_REQUIRED)
    set(_REALTEK_MODE REQUIRED)
endif()

find_package(PkgConfig)
pkg_check_modules(REALTEK ${_REALTEK_MODE} kylin-platform-lib)

include(FindPackageHandleStandardArgs)

message("-- REALTEK_LIBRARIES  ${REALTEK_LIBRARIES}")
message("-- REALTEK_INCLUDE_DIRS ${REALTEK_INCLUDE_DIRS}")

find_package_handle_standard_args(RTKlibs DEFAULT_MSG REALTEK_INCLUDE_DIRS REALTEK_LIBRARIES)
mark_as_advanced(REALTEK_INCLUDE_DIRS REALTEK_LIBRARIES)

if(RTKlibs_FOUND AND NOT TARGET RTKlibs::RTKlibs)
    add_library(RTKlibs::RTKlibs UNKNOWN IMPORTED)
    set_target_properties(RTKlibs::RTKlibs PROPERTIES
            IMPORTED_LOCATION "${REALTEK_LIBRARY}"
            INTERFACE_LINK_LIBRARIES "${REALTEK_LIBRARIES}"
            INTERFACE_COMPILE_OPTIONS "${REALTEK_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${REALTEK_INCLUDE_DIRS}"
            )
endif()
