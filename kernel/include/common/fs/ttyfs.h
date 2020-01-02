#ifndef __FS_TTYFS__
#define __FS_TTYFS__ 1

#include <common/fs/vfs.h>
#include <common/tty/tty.h>

struct ttyfs_inode
{
    struct inode inode;
    tty_dev_t* tty;
};

struct ttyfs_dnode
{
    struct dnode dnode;

    struct ttyfs_dnode *next;
};

struct ttyfs_instance
{
    struct fs_instance instance;
    
    struct ttyfs_dnode *dnodes;
    ino_t next_inode;
};

struct fs_instance* ttyfs_create();

void ttyfs_add_tty(struct ttyfs_instance* instance, tty_dev_t* dev, const char* path);

void ttyfs_destroy(struct fs_instance* instance);

#endif /* __FS_TTYFS__ */