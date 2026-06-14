/* I developed some samples using AI, this one for FUSE demo. */
/* A nice app service can be developed very fast with AI. I felt 1Year=1Month with AI for a considerably big serivce-app */
/* Great tool and saves huge time. Those who better know concepts and internals of OS subsystems... Life made Easy */
/* This is a simple FUSE programme to demo FS for creating only files starting with my_ preix */
/* Install:  libfuse3-dev fuse3 */
/* Compile success: gcc myfs.c -lfuse3 -o myfs */
/* Usage: mkdir mountpoint; ./myfs mntpoint */


#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static const char *dir_path = "/";
static const char *allowed_prefix = "my_";

/**
 * Check if filename starts with "my_"
 */
int is_valid_name(const char *path)
{
    // Skip leading '/'
    if (path[0] == '/')
        path++;

    if (strncmp(path, allowed_prefix, strlen(allowed_prefix)) == 0)
        return 1;

    return 0;
}

/**
 * Get file attributes
 */
static int myfs_getattr(const char *path, struct stat *stbuf,
                       struct fuse_file_info *fi)
{
    (void) fi;

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        // Root directory
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    // Fake file exists if name is valid
    if (is_valid_name(path)) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = 0;
        return 0;
    }

    return -ENOENT;
}

/**
 * Read directory (only shows one example file)
 */
static int myfs_readdir(const char *path, void *buf,
                       fuse_fill_dir_t filler, off_t offset,
                       struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    // Default entries
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    // Example file
    filler(buf, "my_sample.txt", NULL, 0, 0);

    return 0;
}

/**
 * Create file
 */
static int myfs_create(const char *path, mode_t mode,
                       struct fuse_file_info *fi)
{
    (void) mode;
    (void) fi;

    if (!is_valid_name(path)) {
        printf("Blocked creation: %s\n", path);
        return -EACCES;
    }

    printf("Created file: %s\n", path);
    return 0;
}

/**
 * Open file
 */
static int myfs_open(const char *path, struct fuse_file_info *fi)
{
    if (!is_valid_name(path))
        return -ENOENT;

    return 0;
}

/**
 * Write file (dummy)
 */
static int myfs_write(const char *path, const char *buf,
                      size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    (void) path;
    (void) buf;
    (void) offset;
    (void) fi;

    return size;
}

/**
 * FUSE operations
 */
static struct fuse_operations myfs_ops = {
    .getattr = myfs_getattr,
    .readdir = myfs_readdir,
    .create  = myfs_create,
    .open    = myfs_open,
    .write   = myfs_write,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &myfs_ops, NULL);
}
