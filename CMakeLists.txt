cmake_minimum_required(VERSION 3.7.4)

project("K4OS")

# Options
option(ENABLE_ACPI_AML_DEBUGGER
       "Enables the ACPICA AML debugger" OFF)

option(ENABLE_ACPI_AML_DISASM
       "Enables the ACPICA AML disassembler" OFF)

# Generic Definitions
add_definitions(-D__${TARGET_ARCH}__=1)

# Global include directories
include_directories("${PROJECT_SOURCE_DIR}/acpi/source/include")
include_directories("${PROJECT_SOURCE_DIR}/kernel/include")
include_directories("${PROJECT_SOURCE_DIR}/kernel/arch/${TARGET_ARCH}/include")
include_directories("${PROJECT_SOURCE_DIR}/sysroot/usr/local/include")

if(TARGET_ARCH STREQUAL "i386" OR TARGET_ARCH STREQUAL "x86_64")
    include_directories("${PROJECT_SOURCE_DIR}/kernel/arch/x86/include")
endif()

# Arch-dependent definitions
math(EXPR arch_bits "${CMAKE_SIZEOF_VOID_P}*8")
add_definitions(-DARCH_BITS=${arch_bits})

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

# ACPICA Driver
add_subdirectory("acpi/")

add_subdirectory("kernel/")