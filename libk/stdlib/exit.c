#include <stdlib.h>
#include <stdio.h>
#include <kernhooks.h>

__attribute__((__noreturn__))
void exit(int status)
{
#if __STDC_HOSTED__ == 1
    //TODO: Call exit with status
    printf("exit(%d)\n", status);
    while (1);
#else
    kexit(status);
#endif
    __builtin_unreachable();
}
