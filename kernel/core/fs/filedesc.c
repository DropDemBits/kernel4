#include <common/mm/liballoc.h>
#include <common/fs/vfs.h>
#include <common/fs/filedesc.h>
#include <common/util/bitmap.h>
#include <common/util/klog.h>
#include <common/errno.h>

#include <string.h>

/// File descriptor management
// Maximum number of files open
#define MAX_FD 65536
// Number of entries to grow by
#define GROW_BY 256

// Add an entry to the fd_map
// Assumes "fd_lock" has already been acquired
static int add_fd_entry(process_t* process, size_t fd, struct filedesc* filedesc)
{
    if (process->fd_map_len <= fd)
    {
        // Allocate a new fd map, adding 256 entries
        void* oldp = process->fd_map;
        size_t oldsize = process->fd_map_len;

        process->fd_map = krealloc(process->fd_map, sizeof(*process->fd_map) * (GROW_BY + process->fd_map_len));
        if (!process->fd_map)
            return -ENOMEM;

        // Zero new memory
        memset(process->fd_map + process->fd_map_len, 0, sizeof(*process->fd_map) * GROW_BY);

        process->fd_map_len += GROW_BY;
    }

    process->fd_map[fd] = filedesc;

    if (bitmap_test(&process->fd_alloc, fd) == -1)
        bitmap_grow(&process->fd_alloc, GROW_BY);
    bitmap_set(&process->fd_alloc, fd);
    return 0;
}

// Frees the file descriptor number
// Assumes "fd_lock" has already been acquired
// Gives back the old "filedesc" structure
static struct filedesc* remove_fd_entry(process_t* process, size_t fd)
{
    // Not alloc'd? Not an fd
    if (bitmap_test(&process->fd_alloc, fd) != 1)
        return NULL;

    // Free the file descriptor
    bitmap_clear(&process->fd_alloc, fd);

    struct filedesc* filedesc = process->fd_map[fd];
    process->fd_map[fd] = NULL;
    return filedesc;
}

// Creates a file descriptor, returns the "fd" index
int create_filedesc(struct dnode* backing_dnode)
{
    if (!backing_dnode)
        return -ENOENT;

    int error_code = 0;

    process_t* current_process = sched_active_process();
    struct bitmap* fd_alloc = &current_process->fd_alloc;

    mutex_acquire(current_process->fd_lock);

    if (!fd_alloc->bitmaps)
    {
        klog_logln(LVL_DEBUG, "Bitmap make");
        // Initialliy allocate space for "GROW_BY" descriptors
        if (bitmap_init(fd_alloc, GROW_BY))
        {
            error_code = -ENOMEM;
            goto has_error;
        }
    }

    // Look for a free file descriptor index
    size_t use_fd = 0;
    for (;;)
    {
        int test_result = bitmap_test(fd_alloc, use_fd);

        //klog_logln(LVL_DEBUG, "Testing %d (%d)", use_fd, test_result);
        if (test_result <= 0)
        {
            // Bit is free, use that
            break;
        }

        use_fd++;
        if (use_fd >= MAX_FD)
        {
            error_code = -ENFILE;
            goto has_error;
        }
    }

    // Allocate a new file descriptor
    //klog_logln(LVL_DEBUG, "Creating fd for %s (%d)", backing_dnode->name, use_fd);

    struct filedesc* filedesc = kmalloc(sizeof(*filedesc));
    if (!filedesc)
    {
        error_code = -ENOMEM;
        goto has_error;
    }

    filedesc->backing_dnode = backing_dnode;

    // Add to the map
    error_code = add_fd_entry(current_process, use_fd, filedesc);
    if(error_code)
    {
        kfree(filedesc);
        goto has_error;
    }

    // Done, relinquish lock
    mutex_release(current_process->fd_lock);
    return use_fd;

    ///////////////////////////////////////////////////////

    has_error:
    mutex_release(current_process->fd_lock);
    return error_code;
}

// Gets the file descriptor associated with the "fd" index. Returns NULL if not found
struct filedesc* get_filedesc(int fd)
{
    if (fd < 0)
        return NULL;

    process_t* current_process = sched_active_process();
    struct filedesc* filedesc = NULL;

    mutex_acquire(current_process->fd_lock);

    // Outside of entry map? Most likely free
    if ((size_t)fd >= current_process->fd_map_len)
    {
        klog_logln(LVL_DEBUG, "Outside entry size (%d >= %d)", fd, current_process->fd_map_len);
        mutex_release(current_process->fd_lock);
        return NULL;
    }
    
    // Outside of allocated entries or not allocated? Most likely free
    int bit_test = bitmap_test(&current_process->fd_alloc, (size_t)fd);
    if (bit_test != 1)
    {
        klog_logln(LVL_DEBUG, "Outside bitmap alloc (%d)", bit_test);
        mutex_release(current_process->fd_lock);
        return NULL;
    }

    filedesc = current_process->fd_map[(size_t)fd];

    mutex_release(current_process->fd_lock);
    return filedesc;
}

// Frees the file descriptor and associated index
int free_filedesc(int fd)
{
    if (fd < 0)
        return -EBADF;

    process_t* process = sched_active_process();

    mutex_acquire(process->fd_lock);
    struct filedesc* filedesc = remove_fd_entry(process, (size_t)fd);
    mutex_release(process->fd_lock);

    if (filedesc == NULL)
        return -EBADF;

    // Done with the file descriptor
    kfree(filedesc);

    return 0;
}
