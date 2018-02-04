#include <vfs.h>

#ifndef __TARFS_H__
#define __TARFS_H__

vfs_inode_t* tarfs_init(void* address, size_t tarlen);

#endif /* __TARFS_H__ */
