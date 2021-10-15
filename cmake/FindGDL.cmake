
if(GDL_FIND_QUIETLY)
    set(_GDL_MODE QUIET)
elseif(GDL_FIND_REQUIRED)
    set(_GDL_MODE REQUIRED)
endif()

find_package(PkgConfig)
pkg_check_modules(GDL ${_GDL_MODE} gdl)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GDL DEFAULT_MSG GDL_INCLUDE_DIRS GDL_LIBRARIES)
mark_as_advanced(GDL_INCLUDE_DIRS GDL_LIBRARIES)

if(GDL_FOUND AND NOT TARGET GDL::GDL)
    add_library(GDL::GDL UNKNOWN IMPORTED)
    set_target_properties(GDL::GDL PROPERTIES
            IMPORTED_LOCATION "${GDL_LIBRARY}"
            INTERFACE_LINK_LIBRARIES "${GDL_LIBRARIES}"
            INTERFACE_COMPILE_OPTIONS "${GDL_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${GDL_INCLUDE_DIRS}"
            )
endif()
