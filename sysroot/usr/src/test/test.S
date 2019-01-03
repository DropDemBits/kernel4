.section .text
.global _start
_start:
    # Print out test string
    movl $1, %eax
    movl $(strval), %ebx
    int $0x80

    # Sleep for 1 second
    movl $2, %eax
    movl $1000, %ebx
    int $0x80

    # Exit
    movl $3, %eax
    int $0x80
    jmp _start

.section .rodata
strval:
    .asciz "Hello World!\n"
