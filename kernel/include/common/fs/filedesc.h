#ifndef __FILEDESC_H__
#define __FILEDESC_H__ 1

#include <common/fs/vfs.h>

// Representation of a file descriptor
struct filedesc
{
    struct dnode* backing_dnode;
};

/** Creates a file descriptor, returns the "fd" index */
int create_filedesc(struct dnode* backing_dnode);
/** Gets the file descriptor associated with the "fd" index. Returns NULL if not found */
struct filedesc* get_filedesc(int fd);
/** Frees the file descriptor and associated index */
int free_filedesc(int fd);

#endif /* __FILEDESC_H__ */