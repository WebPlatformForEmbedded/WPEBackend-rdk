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
pkg_check_modules(westeros-compositor ${_WESTEROS_MODE} IMPORTED_TARGET GLOBAL westeros-compositor)

if (westeros-compositor_FOUND)
   add_library(Westeros::Westeros ALIAS PkgConfig::westeros-compositor)
endif()
