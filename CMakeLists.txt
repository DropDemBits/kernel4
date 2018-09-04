cmake_minimum_required(VERSION 3.7.4)

project("K4OS")


add_library(libk STATIC IMPORTED GLOBAL)
set_target_properties(
    libk
    PROPERTIES IMPORTED_NO_SONAME
    true
)

set_target_properties(
    libk
    PROPERTIES IMPORTED_LOCATION
    "${CMAKE_SOURCE_DIR}/sysroot/usr/local/lib/${TARGET_ARCH}/libk.a"
)

include(kernel/CMakeLists.txt)