
if(TARGET_ARCH STREQUAL "")
    message(FATAL_ERROR "No architechture specified")
endif()

INCLUDE(CMakeForceCompiler)

set(CMAKE_SYSTEM_PROCESSOR ${TARGET_ARCH})
set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_VERSION "0.0.0-alpha")

set(CMAKE_SYSROOT ${CMAKE_SOURCE_DIR}/sysroot)
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SOURCE_DIR}/sysroot/usr/local/)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

set(CMAKE_C_FLAGS_INIT "-ffreestanding -nostdlib")
set(CMAKE_CXX_FLAGS_INIT "-ffreestanding -nostdlib")

set(LINKER_FLAGS "")

if(TARGET_ARCH STREQUAL "x86_64")
    set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -mcmodel=large -mno-red-zone")
    set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -mcmodel=large -mno-red-zone")
    set(LINKER_FLAGS "${LINKER_FLAGS} -z max-page-size=0x1000")
endif()

#set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT}")

if(TARGET_ARCH MATCHES "i[3-7]86")
    set(TARGET_TRIPLET i686-elf)
else()
    set(TARGET_TRIPLET ${TARGET_ARCH}-elf)
endif()

CMAKE_FORCE_C_COMPILER(${TARGET_TRIPLET}-gcc GNU)
CMAKE_FORCE_CXX_COMPILER(${TARGET_TRIPLET}-g++ GNU)

#set(CMAKE_FIND_ROOT_PATH sysroot)
#set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE)

#string(REPLACE bin/${TARGET_ARCH}-elf-gcc "" toolchain_home TOOLCHAIN_HOME)
