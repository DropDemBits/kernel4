
include_directories(kernel/include)
include_directories(${CMAKE_SOURCE_DIR}/sysroot/usr/local/include)

# Generic Definitions
add_definitions(-D__${TARGET_ARCH}__=1)

enable_language(ASM)

list(APPEND SOURCES
    kernel/core/kernel.c
    kernel/core/syscall.c
    kernel/core/multiboot.c
    kernel/core/mm/mm.c
    kernel/core/mm/liballoc.c
    kernel/core/mm/allochooks.c
    kernel/core/tty/tty.c
    kernel/core/tty/fb_generic.c
    kernel/core/tty/font.c
    kernel/core/khooks.c
    kernel/core/util/klog.c
    kernel/core/util/locks.c
    kernel/core/util/panic.c
    kernel/core/tasks/sched.c
    kernel/core/tasks/tasks.c
    kernel/core/io/pci.c
    kernel/core/io/uart.c
    kernel/core/io/ps2.c
    kernel/core/io/generickbd.c
    kernel/core/io/ps2kbd.c
    kernel/core/io/atkbd.c
    kernel/core/fs/vfs.c
    kernel/core/fs/tarfs.c
    kernel/core/ata/pata.c
    kernel/core/ata/ata.c
    kernel/core/kshell/main.c)

if(TARGET_ARCH STREQUAL "i386" OR TARGET_ARCH STREQUAL "x86_64")
    include(kernel/arch/x86/CMakeLists.txt)
else()
    message(FATAL_ERROR "Unknown architechture " ${TARGET_ARCH})
endif()

# Arch-dependent definitions
math(EXPR arch_bits "${CMAKE_SIZEOF_VOID_P}*8")
add_definitions(-DARCH_BITS=${arch_bits})

# Misc.
set(TARGET_NAME k4-kernel)

add_executable(${TARGET_NAME} ${SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME k4-${TARGET_ARCH}.kern)
set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "-T ${LINKER_FILE} ${LINKER_FLAGS}")
target_link_libraries(${TARGET_NAME} libk "-lgcc")
