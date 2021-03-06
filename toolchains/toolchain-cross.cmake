
if(TARGET_ARCH STREQUAL "")
    message(FATAL_ERROR "No architechture specified")
endif()

set(CMAKE_SYSTEM_PROCESSOR ${TARGET_ARCH})
set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_VERSION "0.0.0-alpha")

set(CMAKE_SYSROOT ${CMAKE_SOURCE_DIR}/sysroot)
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SOURCE_DIR}/sysroot/usr/)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

set(CMAKE_C_COMPILER ${TARGET_TRIPLET}-gcc)
set(CMAKE_CXX_COMPILER ${TARGET_TRIPLET}-g++)

set(CMAKE_C_FLAGS_INIT "-ffreestanding -nostdlib")
set(CMAKE_CXX_FLAGS_INIT "-ffreestanding -nostdlib")

if(TARGET_ARCH STREQUAL "x86_64")
    set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -mcmodel=large -mno-red-zone")
    set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -mcmodel=large -mno-red-zone")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -z max-page-size=0x1000")
endif()

if(TARGET_ARCH MATCHES "i[3-7]86")
    set(TARGET_TRIPLET i686-k4os)
else()
    set(TARGET_TRIPLET ${TARGET_ARCH}-k4os)
endif()

