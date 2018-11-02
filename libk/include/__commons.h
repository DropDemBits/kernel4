#include <stddef.h>

//Commons
#ifdef __cplusplus
extern "C"
{
#endif

#ifdef NULL
#   undef NULL
#   define NULL (void*)0
#endif

#define __EXPORT_SPEC__ extern

#ifdef __cplusplus
}
#endif
