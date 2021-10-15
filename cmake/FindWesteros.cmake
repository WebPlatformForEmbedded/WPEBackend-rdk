# - Try to find westeros.
# Once done, this will define
#
#  WESTEROS_INCLUDE_DIRS - the westeros include directories
#  WESTEROS_LIBRARIES - link these to use westeros.
#
#  Westeros::Westeros, the westeros library
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

if(Westeros_FIND_QUIETLY)
    set(_WESTEROS_MODE QUIET)
elseif(Westeros_FIND_REQUIRED)
    set(_WESTEROS_MODE REQUIRED)
endif()

find_package(PkgConfig)
pkg_check_modules(PC_WESTEROS ${_WESTEROS_MODE} westeros-compositor)
find_path(WESTEROS_INCLUDE_DIRS
    NAMES westeros-compositor.h
    HINTS ${PC_WESTEROS_INCLUDE_DIRS} ${PC_WESTEROS_INCLUDEDIR}
)

find_library(WESTEROS_LIBRARIES
    NAMES ${PC_WESTEROS_LIBRARIES}
    HINTS ${PC_WESTEROS_LIBRARY_DIRS} ${PC_WESTEROS_LIBDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Westeros DEFAULT_MSG WESTEROS_FOUND WESTEROS_INCLUDE_DIRS WESTEROS_LIBRARIES)
mark_as_advanced(WESTEROS_INCLUDE_DIRS WESTEROS_LIBRARIES)

if(Westeros_FOUND AND NOT TARGET Westeros::Westeros)
    add_library(Westeros::Westeros UNKNOWN IMPORTED)
    set_target_properties(Westeros::Westeros PROPERTIES
            IMPORTED_LOCATION "${WESTEROS_LIB}"
            INTERFACE_LINK_LIBRARIES "${WESTEROS_LIBRARIES}"
            INTERFACE_COMPILE_OPTIONS "${WESTEROS_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${WESTEROS_INCLUDE_DIRS}"
            
endif()


