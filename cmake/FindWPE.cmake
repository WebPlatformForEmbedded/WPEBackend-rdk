# - Try to find WPE.
# Once done, this will define
#
#  WPE_FOUND - system has WPE.
#  WPE_INCLUDE_DIRS - the WPE include directories
#  WPE_LIBRARIES - link these to use WPE.
#
# Copyright (C) 2016 Igalia S.L.
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
if(NOT PC_WPE_FOUND)
pkg_search_module(PC_WPE wpe-1.0)
endif()

if(NOT PC_WPE_FOUND)
pkg_search_module(PC_WPE wpe-0.2)
endif()

if(PC_WPE_FOUND)
    set(WPE_VERSION ${PC_WPE_VERSION} CACHE INTERNAL "")
    set(WPE_CFLAGS ${PC_WPE_CFLAGS_OTHER})

    find_path(WPE_INCLUDE_DIRS
        NAMES wpe/wpe.h
        HINTS ${PC_WPE_INCLUDEDIR} ${PC_WPE_INCLUDE_DIRS}
    )

    find_library(WPE_LIBRARIES
        NAMES ${PC_WPE_LIBRARIES}
        HINTS ${PC_WPE_LIBDIR} ${PC_WPE_LIBRARY_DIRS}
    )
else()
    message(FATAL_ERROR "libwpe not found!")
endif()

# Version 1.12.0 is the last release where XKB support was always present
if (WPE_VERSION VERSION_LESS_EQUAL 1.12.0)
    list(APPEND WPE_CFLAGS -DWPE_ENABLE_XKB=1)
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WPE
    FOUND_VAR WPE_FOUND
    REQUIRED_VARS WPE_LIBRARIES WPE_INCLUDE_DIRS
    VERSION_VAR WPE_VERSION
)
mark_as_advanced(WPE_INCLUDE_DIRS WPE_LIBRARIES)

if(WPE_LIBRARIES AND NOT TARGET WPE::WPE)
    add_library(WPE::WPE UNKNOWN IMPORTED GLOBAL)
    set_target_properties(WPE::WPE PROPERTIES
            IMPORTED_LOCATION "${WPE_LIBRARIES}"
            INTERFACE_COMPILE_OPTIONS "${WPE_CFLAGS}"
            INTERFACE_INCLUDE_DIRECTORIES "${WPE_INCLUDE_DIRS}"
    )
endif()
