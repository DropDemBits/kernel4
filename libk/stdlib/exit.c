#include <stdlib.h>
#include <stdio.h>
#if __STDC_HOSTED__ == 0
extern void kexit(int status);
#endif

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
