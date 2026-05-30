# FindLibEbur128.cmake
# libebur128 does not ship a .pc file, so we search manually.
#
# Defines:
#   LibEbur128_FOUND
#   LibEbur128_INCLUDE_DIRS
#   LibEbur128_LIBRARIES
#   LibEbur128::LibEbur128 (imported target)

find_path(LibEbur128_INCLUDE_DIR
    NAMES ebur128.h
    HINTS /ucrt64/include /mingw64/include /usr/local/include /usr/include
    DOC "Directory containing ebur128.h"
)

find_library(LibEbur128_LIBRARY
    NAMES ebur128 libebur128
    HINTS /ucrt64/lib /mingw64/lib /usr/local/lib /usr/lib
    DOC "Path to libebur128"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibEbur128
    REQUIRED_VARS LibEbur128_LIBRARY LibEbur128_INCLUDE_DIR
)

if(LibEbur128_FOUND AND NOT TARGET LibEbur128::LibEbur128)
    add_library(LibEbur128::LibEbur128 UNKNOWN IMPORTED)
    set_target_properties(LibEbur128::LibEbur128 PROPERTIES
        IMPORTED_LOCATION "${LibEbur128_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LibEbur128_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(LibEbur128_INCLUDE_DIR LibEbur128_LIBRARY)
