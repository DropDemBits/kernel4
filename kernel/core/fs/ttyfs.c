#include <common/fs/ttyfs.h>

#include <string.h>

#include <common/mm/liballoc.h>
#include <common/io/kbd.h>
#include <common/util/klog.h>

extern struct dirent* default_dnode_readdir(struct dnode *dnode, size_t index, struct dirent* dirent);
extern struct dnode* default_dnode_finddir(struct dnode *dnode, const char* path);

static struct dnode_ops ttyfs_dops =
{
    .read_dir = default_dnode_readdir,
    .find_dir = default_dnode_finddir,
};

static void construct_dnode(struct ttyfs_instance *instance, struct dnode *dnode, struct inode *inode, const char* path)
{
    dnode->instance = (struct fs_instance*)instance;
    dnode->inode = inode;
    dnode->path = path;
    dnode->name = strrchr(path, '/');
    if(dnode->name == NULL)
        dnode->name = path;
    dnode->parent = NULL;
    dnode->subdirs = NULL;
    dnode->next = NULL;

    dnode->dops = &ttyfs_dops;
}

extern void default_open(struct inode *file_node, int oflags);
extern void default_close(struct inode *file_node);


// Reads bytes from the node
static ssize_t ttyfs_read(struct inode *file_node, size_t off, size_t len, void* buffer)
{
    if((file_node->type & 7) == VFS_TYPE_DIRECTORY)
        return 0;   // No reading for u.

    return 0;
}

static ssize_t ttyfs_write(struct inode *file_node, size_t off, size_t len, void* buffer)
{
    if((file_node->type & 7) == VFS_TYPE_DIRECTORY)
        return 0;   // No writting for u.

    struct ttyfs_inode* tty_node = (struct ttyfs_inode*)file_node;

    // Write it out! (off has no effect)
    for(size_t i = 0; i < len; i++)
        tty_putchar(tty_node->tty, ((const char*)buffer)[i]);

    return len;
}

static struct inode_ops ttyfs_iops = 
{
    .open = default_open,
    .close = default_close,
    .read = ttyfs_read,
    .write = ttyfs_write,
};

static void construct_inode(struct ttyfs_instance *instance, struct inode *inode)
{
    inode->instance = (struct fs_instance*)instance;
    inode->num_users = 0;
    inode->num_users_lock = mutex_create();
    inode->iops = &ttyfs_iops;
    inode->perms_mask = 0;
    inode->size = 0;
    inode->symlink_ptr = NULL;
    inode->gid = 0;
    inode->uid = 0;

    inode->fs_inode = instance->next_inode++;
}

struct fs_instance* ttyfs_create()
{
    struct ttyfs_instance *instance = kmalloc(sizeof(struct ttyfs_instance));
    struct dnode *root_dir = kmalloc(sizeof(struct dnode));
    struct inode *root_inode = kmalloc(sizeof(struct inode));

    construct_dnode(instance, (struct dnode*)root_dir, root_inode, "/");
    construct_inode(instance, (struct inode*)root_inode);

    root_inode->type = VFS_TYPE_DIRECTORY;

    instance->instance.root = root_dir;

    return (struct fs_instance*)instance;
}

void ttyfs_add_tty(struct ttyfs_instance* instance, tty_dev_t* dev, const char* path)
{
    // Find parent dir to connect to (temp: just attatch to root)
    struct dnode *parent_dir = instance->instance.root;

    // Construct tty_entry
    struct ttyfs_dnode *dnode = kmalloc(sizeof(struct ttyfs_dnode));
    struct ttyfs_inode *inode = kmalloc(sizeof(struct ttyfs_inode));

    construct_inode(instance, (struct inode*)inode);
    construct_dnode(instance, (struct dnode*)dnode, (struct inode*)inode, path);

    inode->inode.type = VFS_TYPE_CHARDEV;
    inode->tty = dev;

    // Append to dnode list
    dnode->next = instance->dnodes;
    instance->dnodes = dnode;

    // Append it to the parent dir
    dnode->dnode.next = parent_dir->subdirs;
    parent_dir->subdirs = (struct dnode*)dnode;
}

void ttyfs_destroy(struct fs_instance* instance)
{
    //kfree(instance);
}
