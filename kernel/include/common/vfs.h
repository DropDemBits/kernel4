#include <common/types.h>

#ifndef __VFS_H__
#define __VFS_H__

#define VFS_TYPE_FILE 0x01
#define VFS_TYPE_DIRECTORY 0x02
#define VFS_TYPE_CHARDEV 0x03
#define VFS_TYPE_BLOCKDEV 0x04
#define VFS_TYPE_PIPE 0x05
#define VFS_TYPE_SYMLINK 0x06
#define VFS_TYPE_MOUNT 0x08

#define VFSO_RDONLY	0x01
#define VFSO_WRONLY	0x02
#define VFSO_RDWR 	0x03

struct vfs_inode;
struct vfs_dirent;

// Reads bytes from the node
typedef ssize_t (*vfs_read_func_t)(struct vfs_inode *node, size_t off, size_t len, uint8_t* buffer);
// Writes bytes to the node
typedef ssize_t (*vfs_write_func_t)(struct vfs_inode *node, size_t off, size_t len, uint8_t* buffer);
// Opens a node for use
typedef void (*vfs_open_func_t)(struct vfs_inode *node, int oflags);
// Closes a node
typedef void (*vfs_close_func_t)(struct vfs_inode *node);
// Gets a dirent from an index
typedef struct vfs_dirent* (*vfs_readdir_func_t)(struct vfs_inode *node, size_t index);
// Gets a vfs_inode from a name
typedef struct vfs_inode* (*vfs_finddir_func_t)(struct vfs_inode *node, const char* name);

typedef struct vfs_inode {
	// name in dirent
	uint32_t perms_mask : 12; // Access permissions
	uint32_t type : 4; // Node type
	uint32_t uid;	// User ID
	uint32_t gid;	// Group ID
	uint32_t size;	// Size in bytes
	ino_t inode; // Associated fs inode
	vfs_read_func_t read;
	vfs_write_func_t write;
	vfs_open_func_t open;
	vfs_close_func_t close;
	vfs_readdir_func_t readdir;
	vfs_finddir_func_t finddir;
	uint32_t impl_specific; // VFS-impl specific value
	struct vfs_inode* ptr;
} vfs_inode_t;

struct vfs_dirent {
	ino_t inode; // Associated vfs_inode
	char* name;
};

void vfs_mount(vfs_inode_t* root_node, const char* path);
vfs_inode_t* vfs_getrootnode(const char* path);
void vfs_setroot(vfs_inode_t* root_node, const char* path);
vfs_inode_t* vfs_getroot();
ssize_t vfs_read(vfs_inode_t *root_node, size_t off, size_t len, uint8_t* buffer);
ssize_t vfs_write(vfs_inode_t *root_node, size_t off, size_t len, uint8_t* buffer);
void vfs_open(vfs_inode_t *file_node, int oflags);
void vfs_close(vfs_inode_t *file_node);
struct vfs_dirent* vfs_readdir(vfs_inode_t *root_node, size_t index);
vfs_inode_t* vfs_finddir(vfs_inode_t *root_node, const char* name);

#endif /* __VFS_H__ */
