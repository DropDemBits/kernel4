#include <common/types.h>
#include <sys/types.h>

#ifndef __VFS_H__
#define __VFS_H__

#define MAX_PATHLEN 4096

#define VFS_TYPE_UNKNOWN 0x00
#define VFS_TYPE_FILE 0x01
#define VFS_TYPE_DIRECTORY 0x02
#define VFS_TYPE_CHARDEV 0x03
#define VFS_TYPE_BLOCKDEV 0x04
#define VFS_TYPE_PIPE 0x05
#define VFS_TYPE_SYMLINK 0x06
#define VFS_TYPE_MOUNT 0x08

#define VFSO_RDONLY    0x01
#define VFSO_WRONLY    0x02
#define VFSO_RDWR     0x03

struct vfs_inode;
struct vfs_dir;
struct vfs_fs_instance;
struct dirent;

// Reads bytes from the node
typedef ssize_t (*vfs_read_func_t)(struct vfs_inode *file_node, size_t off, size_t len, void* buffer);
// Writes bytes to the node
typedef ssize_t (*vfs_write_func_t)(struct vfs_inode *file_node, size_t off, size_t len, void* buffer);
// Opens a node for use
typedef void (*vfs_open_func_t)(struct vfs_inode *file_node, int oflags);
// Closes a node
typedef void (*vfs_close_func_t)(struct vfs_inode *file_node);
/**
 * Gets a dirent from an index. Returns NULL if there are no other dirents
 * Index is the file number in the node
 * Node is the directory to start searching from
 * Dirent is the dirent to fill up and is the one returned
 */
typedef struct dirent* (*vfs_readdir_func_t)(struct vfs_inode *node, size_t index, struct dirent* dirent);
/** 
 * Gets a vfs_inode from a name. Returns NULL if not found
 * May create a new vfs inode
 */
typedef struct vfs_dir* (*vfs_finddir_func_t)(struct vfs_dir *dnode, const char* name);

// fs_instance
typedef struct vfs_inode* (*vfs_get_inode_t)(struct vfs_fs_instance *instance, ino_t inode);

struct vfs_inode_ops
{
    vfs_read_func_t read;
    vfs_write_func_t write;
    vfs_open_func_t open;
    vfs_close_func_t close;
};

struct vfs_dnode_ops
{
    vfs_readdir_func_t read_dir;
    vfs_finddir_func_t find_dir;
};

typedef struct vfs_inode {
    // name in dirent
    uint32_t perms_mask : 12; // Access permissions
    uint32_t type : 4; // Node type
    uint32_t uid;    // User ID
    uint32_t gid;    // Group ID
    uint32_t size;    // File size in bytes
    ino_t fs_inode; // Associated fs inode
    struct vfs_inode_ops *iops;
    uint32_t num_users;    // Number of users that this file has
    uint32_t impl_specific; // VFS-impl specific value
    struct vfs_inode* symlink_ptr; // Pointer to the target symlink inode
} vfs_inode_t;

struct dirent {
    ino_t inode; // Associated vfs_inode
    const char* name;
};

struct vfs_dir {
    struct vfs_dir* parent;     // Canonical parent directory (can be null)
    struct vfs_dir* subdirs;    // Subdirectory child (points to first child)
    struct vfs_dir* next;       // Next directory in parent directory

    const char* path;   // Full path to the file/directory
    const char* name;   // Name of the file/directory

    ino_t inode; // Backing inode
    struct vfs_dnode_ops* dops;
    struct vfs_fs_instance* instance; // Associated instance
};

struct vfs_fs_instance
{
    struct vfs_dir* root;
    vfs_get_inode_t get_inode;
};

struct vfs_mount
{
    struct vfs_fs_instance* instance;
    const char* path;
};

void vfs_mount(struct vfs_fs_instance* mount, const char* path);
struct vfs_mount* vfs_get_mount(const char* path);
ssize_t vfs_read(vfs_inode_t *root_node, size_t off, size_t len, void* buffer);
ssize_t vfs_write(vfs_inode_t *root_node, size_t off, size_t len, void* buffer);
void vfs_open(vfs_inode_t *file_node, int oflags);
void vfs_close(vfs_inode_t *file_node);
struct dirent* vfs_readdir(struct vfs_dir *root, size_t index, struct dirent* dirent);
struct vfs_dir* vfs_find_dir(struct vfs_dir* root, const char* path);

#endif /* __VFS_H__ */
