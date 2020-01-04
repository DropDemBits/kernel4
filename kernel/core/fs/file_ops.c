// System call interface for files
#include <common/fs/vfs.h>
#include <common/fs/filedesc.h>
#include <common/errno.h>

int do_open(const char* file_path, int oflags, int mode)
{
    // ???: Why are there no "fs_type"s? (holds inode_ops, dnode_ops, file_ops, other fs_type info)
    // Find corresponding inode (vfs_finddir)

    // Right now, assume all paths are from the root directory of the root mount
    // TODO: Deal with current working directory (per-process)

    struct vfs_mount* fs_mount = vfs_get_mount("/");
    if (!fs_mount)
        return -ENOENT;

    struct dnode* root = fs_mount->instance->root;
    struct dnode* target_dnode = vfs_finddir(root, file_path);

    if (!target_dnode)
    {
        // Change the error code based on the open mode
        // TODO: Create new files if the fs allows it

        if (oflags & VFSO_WRONLY)
            return -EROFS;
        else
            return -ENOENT;
    }

    // Open corresponding inode (vfs_open)
    vfs_open(to_inode(target_dnode), oflags);

    // Create filedesc with fs's file_ops (create_filedesc)
    // "create_filedesc" already handles the fileops part of the equation
    int fd = create_filedesc(target_dnode);
    // fd contains the specific error given by "create_filedesc"
    if (fd < 0)
        return fd;

    // TODO: target_dnode may have been allocated by the fs, and thus becomes a dangling reference here. Deal with that

    return fd;
}

int do_close(int fd)
{
    struct filedesc* filedesc = get_filedesc(fd);
    if (filedesc == NULL)
        return -EBADF; // File descriptor never existed

    // Get backing dnode
    struct dnode* target_dnode = filedesc->backing_dnode;

    // Free the file descriptor
    int status = free_filedesc(fd);
    if (status)
        return status;

    // Close the backing inode
    vfs_close(to_inode(target_dnode));

    // TODO: what to do about dnodes?

    return 0;
}
