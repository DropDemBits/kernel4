#include <__commons.h>
#include <sys/types.h>

#ifndef __UNISTD_H__
#define __UNISTD_H__ 1

__EXPORT_SPEC__ pid_t fork();
__EXPORT_SPEC__ int execv(const char *path, char *const argv[]);
__EXPORT_SPEC__ int execvp(const char *path, char *const argv[]);
__EXPORT_SPEC__ int execve(const char *path, char *const argv[], char *const envp[]);

#endif /* __UNISTD_H__ */