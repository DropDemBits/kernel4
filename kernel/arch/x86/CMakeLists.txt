
# Definitions
add_definitions(-D__X86__=1)

list(APPEND SOURCES
    arch/x86/io/pci_io.c
    arch/x86/io/ps2.c
    arch/x86/io/uart.c
    arch/x86/hal.c
    arch/x86/pic.c
    arch/x86/pit.c
    arch/x86/syscall.c
    arch/x86/vga.c
)

list(APPEND SOURCES
    arch/${TARGET_ARCH}/boot/boot.S
    arch/${TARGET_ARCH}/tasks/tasks.c
    arch/${TARGET_ARCH}/tasks/switch_stack.S
    arch/${TARGET_ARCH}/idt.c
    arch/${TARGET_ARCH}/mmu.c
    arch/${TARGET_ARCH}/isr_entry.S
)

# x86_64 only
if(TARGET_ARCH STREQUAL "x86_64")
    list(APPEND SOURCES
        arch/x86_64/boot/bootstrap.S
    )
endif()

set(LINKER_FILE "${CMAKE_SOURCE_DIR}/kernel/arch/${TARGET_ARCH}/linker.ld")