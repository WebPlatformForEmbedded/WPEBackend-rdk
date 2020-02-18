# - Try to Find DirectFB
# Once done, this will define
#
#  DIRECTFB_FOUND - system has DirectFB.
#  DIRECTFB_INCLUDE_DIRS - the DirectFB include directories
#  DIRECTFB_LIBRARIES - link these to use DirectFB.
#
# Copyright (C) 2014 Igalia S.L.
# Copyright (C) 1994-2020 OpenTV, Inc. and Nagravision S.A.
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

pkg_check_modules(PC_DIRECTFB directfb)

if (PC_DIRECTFB_FOUND)
    set(DIRECTFB_DEFINITIONS ${PC_DIRECTFB_CFLAGS_OTHER})
endif ()

find_path(DIRECTFB_INCLUDE_DIRS NAMES directfb.h
    HINTS ${PC_DIRECTFB_INCLUDEDIR} ${PC_DIRECTFB_INCLUDE_DIRS}
)

find_library(DIRECTFB_LIBRARIES directfb
    HINTS ${PC_DIRECTFB_LIBDIR} ${PC_DIRECTFB_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DIRECTFB DEFAULT_MSG DIRECTFB_INCLUDE_DIRS DIRECTFB_LIBRARIES)

mark_as_advanced(DIRECTFB_INCLUDE_DIRS DIRECTFB_LIBRARIES)

