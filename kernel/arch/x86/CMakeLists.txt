
include_directories(kernel/arch/x86/include)
include_directories(kernel/arch/${TARGET_ARCH}/include)

list(APPEND SOURCES
    kernel/arch/x86/io/ps2.c
    kernel/arch/x86/io/uart.c
    kernel/arch/x86/pic.c
    kernel/arch/x86/pit.c
    kernel/arch/x86/syscall.c
    kernel/arch/x86/vga.c
)

list(APPEND SOURCES
    kernel/arch/${TARGET_ARCH}/boot/boot.S
    kernel/arch/${TARGET_ARCH}/tasks/tasks.c
    kernel/arch/${TARGET_ARCH}/tasks/switch_stack.S
    kernel/arch/${TARGET_ARCH}/hal.c
    kernel/arch/${TARGET_ARCH}/idt.c
    kernel/arch/${TARGET_ARCH}/mmu.c
    kernel/arch/${TARGET_ARCH}/isr_entry.S
)

# x86_64 only
if(TARGET_ARCH STREQUAL "x86_64")
    list(APPEND SOURCES
        kernel/arch/x86_64/boot/bootstrap.S
    )
endif()

set(LINKER_FILE "${CMAKE_SOURCE_DIR}/kernel/arch/${TARGET_ARCH}/linker.ld")