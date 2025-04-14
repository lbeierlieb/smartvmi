# Use pkg-config to get paths
find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_LIBVMI REQUIRED libvmi)

# Create an imported target using pkg-config variables
if(NOT TARGET libvmi::libvmi)
    add_library(libvmi::libvmi SHARED IMPORTED)
    set_target_properties(libvmi::libvmi PROPERTIES
        IMPORTED_LOCATION "${PC_LIBVMI_LIBRARY_DIRS}/libvmi.so"
        INTERFACE_INCLUDE_DIRECTORIES "${PC_LIBVMI_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${PC_LIBVMI_LDFLAGS_OTHER}"
    )
endif()
