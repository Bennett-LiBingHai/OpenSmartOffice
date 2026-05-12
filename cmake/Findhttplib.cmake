find_path(httplib_INCLUDE_DIR httplib.h)
find_library(httplib_LIBRARY cpp-httplib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(httplib
    REQUIRED_VARS httplib_INCLUDE_DIR httplib_LIBRARY
)

if(httplib_FOUND AND NOT TARGET httplib::httplib)
    add_library(httplib::httplib UNKNOWN IMPORTED)
    set_target_properties(httplib::httplib PROPERTIES
        IMPORTED_LOCATION "${httplib_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${httplib_INCLUDE_DIR}"
    )
endif()
