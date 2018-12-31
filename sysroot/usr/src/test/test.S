.section .text
.global _start
_start:
    # Print out test string
    xor %eax, %eax
    movl $1, %eax
    movl $(strval), %ebx
    int $0x80

    # Sleep for 1 second
    xor %eax, %eax
    movl $2, %eax
    movl $1000, %ebx
    int $0x80

    # Yield
    xor %eax, %eax
    int $0x80
    jmp _start

.section .rodata
strval:
    .asciz "Hello World! "

# Doing: ELF Parsing