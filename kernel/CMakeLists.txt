
include_directories(kernel/include)
include_directories(${CMAKE_SOURCE_DIR}/sysroot/usr/local/include)

add_definitions(-D__${TARGET_ARCH}__)

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
    kernel/core/util/locks.c
    kernel/core/util/panic.c
    kernel/core/io/uart.c
    kernel/core/tasks/sched.c
    kernel/core/tasks/tasks.c
    kernel/core/io/ps2.c
    kernel/core/io/generickbd.c
    kernel/core/io/ps2kbd.c
    kernel/core/io/atkbd.c
    kernel/core/fs/vfs.c
    kernel/core/fs/tarfs.c
    kernel/core/kshell/main.c)

if(TARGET_ARCH STREQUAL "i386" OR TARGET_ARCH STREQUAL "x86_64")
    include(kernel/arch/x86/CMakeLists.txt)
else()
    message(FATAL_ERROR "Unknown architechture " ${TARGET_ARCH})
endif()

add_executable(k4-${TARGET_ARCH}.kern ${SOURCES})
target_link_libraries(k4-${TARGET_ARCH}.kern libk "-T ${LINKER_FILE} -lgcc")
