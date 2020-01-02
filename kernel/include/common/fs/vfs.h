#ifndef __VFS_H__
#define __VFS_H__ 1

#include <common/types.h>
#include <sys/types.h>

#include <common/util/locks.h>

#define MAX_PATHLEN 4096

#define VFS_TYPE_UNKNOWN    0x00
#define VFS_TYPE_FILE       0x01
#define VFS_TYPE_DIRECTORY  0x02
#define VFS_TYPE_CHARDEV    0x03
#define VFS_TYPE_BLOCKDEV   0x04
#define VFS_TYPE_PIPE       0x05
#define VFS_TYPE_SYMLINK    0x06

#define VFS_TYPE_MASK 0x7

#define VFS_TYPE_MOUNT 0x08

#define VFSO_RDONLY    0x01
#define VFSO_WRONLY    0x02
#define VFSO_RDWR     0x03

struct inode;
struct dnode;
struct fs_instance;
struct dirent;

// Reads bytes from the node
typedef ssize_t (*vfs_read_func_t)(struct inode *file_node, size_t off, size_t len, void* buffer);
// Writes bytes to the node
typedef ssize_t (*vfs_write_func_t)(struct inode *file_node, size_t off, size_t len, void* buffer);
// Opens a node for use
typedef void (*vfs_open_func_t)(struct inode *file_node, int oflags);
// Closes a node
typedef void (*vfs_close_func_t)(struct inode *file_node);
/**
 * Gets a dirent from an index. Returns NULL if there are no other dirents
 * Index is the file number in the node
 * Node is the directory to start searching from
 * Dirent is the dirent to fill up and is the one returned
 */
typedef struct dirent* (*vfs_readdir_func_t)(struct dnode *dnode, size_t index, struct dirent* dirent);
/** 
 * Gets a vfs_inode from a name. Returns NULL if not found
 * May create a new vfs inode
 */
typedef struct dnode* (*vfs_finddir_func_t)(struct dnode *dnode, const char* name);

// fs_instance
typedef struct inode* (*vfs_get_inode_t)(struct fs_instance *instance, ino_t inode);

struct inode_ops
{
    vfs_read_func_t read;
    vfs_write_func_t write;
    vfs_open_func_t open;
    vfs_close_func_t close;
};

struct dnode_ops
{
    vfs_readdir_func_t read_dir;
    vfs_finddir_func_t find_dir;
};

// Represents file permissions & data
struct inode {
    // name in dnode
    uint32_t perms_mask : 12; // Access permissions
    uint32_t type : 4; // Node type
    uint32_t uid;    // User ID
    uint32_t gid;    // Group ID
    uint32_t size;    // File size in bytes
    ino_t fs_inode; // Associated fs inode
    struct inode_ops *iops;
    uint32_t impl_specific; // VFS-impl specific value
    struct inode* symlink_ptr; // Pointer to the target symlink inode

    struct fs_instance* instance;

    uint32_t num_users;    // Number of users that this file has
    mutex_t* num_users_lock;
};

// Represents a directory or the name of one
struct dnode {
    struct dnode* parent;     // Canonical parent directory (can be null)
    struct dnode* subdirs;    // Subdirectory child (points to first child)
    struct dnode* next;       // Next directory in parent directory

    const char* path;   // Full path to the file/directory
    const char* name;   // Name of the file/directory

    struct inode* inode; // Backing inode
    struct dnode_ops* dops;
    struct fs_instance* instance; // Associated instance
};

// Represents an instance of a filesystem
struct fs_instance
{
    struct dnode* root;
    vfs_get_inode_t get_inode;
};

// Describes a mounted filesystem
struct vfs_mount
{
    struct fs_instance* instance;
    const char* path;

    // To the next mount
    struct vfs_mount *next;
};

struct dirent {
    ino_t inode; // Associated vfs_inode
    const char* name;
};

void vfs_mount(struct fs_instance* mount, const char* path);
/** Finds the mount associated with the given path. Returns NULL on failure */
struct vfs_mount* vfs_get_mount(const char* path);
ssize_t vfs_read(struct inode *root_node, size_t off, size_t len, void* buffer);
ssize_t vfs_write(struct inode *root_node, size_t off, size_t len, void* buffer);
void vfs_open(struct inode *file_node, int oflags);
void vfs_close(struct inode *file_node);
struct dirent* vfs_readdir(struct dnode *root, size_t index, struct dirent* dirent);
/** Walks the path to find the given dnode. Returns NULL on failure */
struct dnode* vfs_finddir(struct dnode* root, const char* path);
struct inode* to_inode(struct dnode* dnode);

#endif /* __VFS_H__ */
