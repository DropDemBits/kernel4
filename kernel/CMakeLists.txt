
include_directories(include)

add_definitions(-D__KERNEL__)

enable_language(ASM)

list(APPEND SOURCES
    core/kernel.c
    core/syscall.c
    core/multiboot.c
    core/elf.c
    core/mm/mm.c
    core/mm/liballoc.c
    core/mm/allochooks.c
    core/tty/tty.c
    core/tty/fb_generic.c
    core/tty/font.c
    core/khooks.c
    core/util/klog.c
    core/util/locks.c
    core/util/panic.c
    core/tasks/sched.c
    core/tasks/tasks.c
    core/io/pci.c
    core/io/uart.c
    core/io/ps2.c
    core/io/generickbd.c
    core/io/ps2kbd.c
    core/fs/vfs.c
    core/fs/tarfs.c
    core/fs/generic_iops.c
    #core/fs/fileops.c
    core/fs/ttyfs.c
    core/ata/pata.c
    core/ata/ata.c
    core/kshell/main.c
    core/acpi/osl.c
    core/acpi/ec.c
    core/acpi/acpi.c
    core/ipc/message.c
    core/ipc/ipctest.c
    core/usb/usbcore.c
    core/hal/timer.c)

if(TARGET_ARCH STREQUAL "i386" OR TARGET_ARCH STREQUAL "x86_64")
    include(arch/x86/CMakeLists.txt)
else()
    message(FATAL_ERROR "Unknown architechture " ${TARGET_ARCH})
endif()

# Misc.
set(TARGET_NAME k4-kernel)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")

add_executable(${TARGET_NAME} ${SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME k4-${TARGET_ARCH}.kern)
set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "-T ${LINKER_FILE}")# ${LINKER_FLAGS}")
target_link_libraries(${TARGET_NAME} acpi libk "-lgcc")
