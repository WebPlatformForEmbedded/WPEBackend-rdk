# - Try to Find DirectFB
# Once done, this will define
#
#  DIRECTFB_FOUND - system has DirectFB installed.
#  DIRECTFB_INCLUDE_DIRS - directories which contain the DirectFB headers.
#  DIRECTFB_LIBRARIES - libraries required to link against DirectFB.
#  DIRECTFB_DEFINITIONS - Compiler switches required for using DirectFB.
#


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

