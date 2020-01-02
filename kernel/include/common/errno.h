#ifndef __K4_ERRNO_H__
#define __K4_ERRNO_H__ 1

// Definitions of error numbers used in the kernel. All number are positive
// and must be negated if desired
#define EINVAL      1
#define EBUSY       2
#define EABSENT     3
#define EINVAL_ARG  4
#define ENOENT      10
#define EROFS       11
#define EBADF       12

#endif /* __K4_ERRNO_H__ */