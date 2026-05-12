# Findlibzip.cmake
# 查找libzip库并创建导入目标 libzip::zip
# 绕过系统自带的libzip CMake配置文件，该配置可能因缺少可选工具（zipcmp、zipmerge、ziptool）而失效

find_path(LIBZIP_INCLUDE_DIR
    NAMES zip.h
    PATH_SUFFIXES include
)

find_library(LIBZIP_LIBRARY
    NAMES zip libzip
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libzip
    REQUIRED_VARS LIBZIP_LIBRARY LIBZIP_INCLUDE_DIR
)

if(libzip_FOUND AND NOT TARGET libzip::zip)
    add_library(libzip::zip UNKNOWN IMPORTED)
    set_target_properties(libzip::zip PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LIBZIP_INCLUDE_DIR}"
        IMPORTED_LOCATION "${LIBZIP_LIBRARY}"
    )
endif()
