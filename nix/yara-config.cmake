# Use pkg-config to get paths
find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_LIBYARA REQUIRED yara)

# Create an imported target using pkg-config variables
if(NOT TARGET unofficial::libyara::libyara)
    add_library(unofficial::libyara::libyara SHARED IMPORTED)
    set_target_properties(unofficial::libyara::libyara PROPERTIES
        IMPORTED_LOCATION "${PC_LIBYARA_LIBRARY_DIRS}/libyara.so"
        INTERFACE_INCLUDE_DIRECTORIES "${PC_LIBYARA_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${PC_LIBYARA_LDFLAGS_OTHER}"
    )
endif()
