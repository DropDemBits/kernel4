#include <stdlib.h>
#include <kernhooks.h>

__attribute__((__noreturn__))
void abort(void)
{
#if __STDC_HOSTED__ == 1
    //TODO: Raise abort signal
    printf("abort()\n");
    while (1);
#else
    kabort();
#endif
    __builtin_unreachable();
}
