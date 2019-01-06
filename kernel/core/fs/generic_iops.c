#include <common/fs/vfs.h>

#include <string.h>

#include <common/util/klog.h>

/**
 * @brief Implements generic dnode & inode operations
 * 
 */

// extern struct dirent* default_dnode_readdir(struct dnode *dnode, size_t index, struct dirent* dirent);
// extern struct dnode* default_dnode_finddir(struct dnode *dnode, const char* path);

struct dirent* default_dnode_readdir(struct dnode *dnode, size_t index, struct dirent* dirent)
{
    // Don't bother for dnode that isn't a directory
    if((dnode->inode->type & 7) != VFS_TYPE_DIRECTORY)
        return NULL;

    // Index 0 & 1 are "." and "..", 2 are the actual directories
    switch(index)
    {
        case 0:
            dirent->inode = dnode->inode->fs_inode;
            dirent->name = ".";
            goto finished;
        case 1:
            if(dnode->parent != NULL)
                dirent->inode = dnode->parent->inode->fs_inode;
            else
                dirent->inode = dnode->inode->fs_inode;
            dirent->name = "..";
            goto finished;
    };

    // Go through all of the directories
    struct dnode* child = dnode->subdirs;

    // Base at zero
    index -= 2;

    // Keep going until the index is zero, or we hit the end
    for(; child != NULL && index; index--, child = child->next);

    // By now, we should have the child node

    // Reached the end of the directory list
    if(child == NULL)
        return NULL;

    // Fill up dirent
    dirent->inode = child->inode->fs_inode;
    dirent->name = child->name;

    finished:
    return dirent;
}

static struct dnode* find_dir(struct dnode *base, const char* path)
{
    if(base == NULL || path == NULL)
        return NULL;

    // Walk through all dirs, starting at the base dir
    char path_components[MAX_PATHLEN];
    char *component = NULL, *last = NULL;
    struct dnode* dir = base;

    memcpy(path_components, path, strlen(path));
    path_components[strlen(path)] = '\0';

    // Check if root is the target dir
    if(strcmp(path, dir->name) == 0)
        return dir;

    component = strtok_r(path_components, "/", &last);

    // Root isn't target dir, so continue
    do
    {
        bool found_path = false;

        for(struct dnode *child = dir->subdirs; child != NULL; child = child->next)
        {
            if(strcmp(child->name, component) == 0)
            {
                dir = child;
                found_path = true;
                break;
            }
        }

        if(found_path)
            continue;

        // No match found, so exit
        dir = NULL;
        break;
    } while((component = strtok_r(NULL, "/", &last)) != NULL);

    return dir;
}

struct dnode* default_dnode_finddir(struct dnode *dnode, const char* path)
{
    if(strcmp(path, ".") == 0)
        return dnode;
    else if(strcmp(path, "..") == 0)
    {
        if(dnode->parent != NULL)
            return dnode->parent;

        // Went up to the root
        return dnode;
    }

    // Find the path now
    struct dnode *dir = find_dir(dnode, path);

    if(!dir)
        return NULL;

    return dir;
}

// extern ssize_t default_read(struct inode *file_node, size_t off, size_t len, void* buffer);
// extern ssize_t default_write(struct inode *file_node, size_t off, size_t len, void* buffer);
// extern void default_open(struct inode *file_node, int oflags);
// extern void default_close(struct inode *file_node);

// No reading
ssize_t default_read(struct inode *file_node, size_t off, size_t len, void* buffer)
{
    return 0;
}

// No writting
ssize_t default_write(struct inode *file_node, size_t off, size_t len, void* buffer)
{
    return 0;
}

// Increment users
void default_open(struct inode *file_node, int oflags)
{
    mutex_acquire(file_node->num_users_lock);
    file_node->num_users++;
    mutex_release(file_node->num_users_lock);
}

// Decrement users
void default_close(struct inode *file_node)
{
    mutex_acquire(file_node->num_users_lock);
    file_node->num_users--;
    mutex_release(file_node->num_users_lock);
}
