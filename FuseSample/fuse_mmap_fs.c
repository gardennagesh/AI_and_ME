/* File system based on user access for a file */
/* Sample to show memmory map the files which threshold frequency access cross 10 a day */
/* CAT any file in this FS and after 10 time a file will be memory mapped */
/* Compiled success: gcc fuse_mmap_fs.c -lfuse3 -o fuse_mmap */

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_FILES 1000
#define MMAP_THRESHOLD 10

typedef struct {
    char path[256];
    int count;
    time_t last_reset;
    void *mapped_data;
    size_t size;
} file_entry;

file_entry files[MAX_FILES];


// Find or create file entry
file_entry* get_entry(const char *path) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(files[i].path, path) == 0)
            return &files[i];
    }

    // create new
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].path[0] == '\0') {
            strcpy(files[i].path, path);
            files[i].count = 0;
            files[i].last_reset = time(NULL);
            files[i].mapped_data = NULL;
            return &files[i];
        }
    }
    return NULL;
}


// Reset counter daily
void reset_if_needed(file_entry *entry) {
    time_t now = time(NULL);
    if (difftime(now, entry->last_reset) > 86400) {
        entry->count = 0;
        entry->last_reset = now;
    }
}


// Memory map file
void map_file(file_entry *entry, const char *fullpath) {
    if (entry->mapped_data != NULL) return;

    int fd = open(fullpath, O_RDONLY);
    if (fd == -1) return;

    struct stat st;
    fstat(fd, &st);
    entry->size = st.st_size;

    entry->mapped_data = mmap(NULL, entry->size,
                              PROT_READ, MAP_SHARED, fd, 0);

    close(fd);
}


// Get Full Path
void get_full_path(char *fullpath, const char *path) {
    strcpy(fullpath, "/tmp");  // backend directory
    strcat(fullpath, path);
}


// getattr
static int my_getattr(const char *path, struct stat *stbuf) {
    char fullpath[512];
    get_full_path(fullpath, path);
    return lstat(fullpath, stbuf);
}


// open
static int my_open(const char *path, struct fuse_file_info *fi) {
    char fullpath[512];
    get_full_path(fullpath, path);

    file_entry *entry = get_entry(path);
    reset_if_needed(entry);

    entry->count++;

    // If threshold reached → mmap
    if (entry->count >= MMAP_THRESHOLD) {
        map_file(entry, fullpath);
        printf("File %s is now memory-mapped\n", path);
    }

    return 0;
}


// read
static int my_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi) {

    char fullpath[512];
    get_full_path(fullpath, path);

    file_entry *entry = get_entry(path);

    // Serve from mmap if available
    if (entry && entry->mapped_data) {
        if (offset < entry->size) {
            if (offset + size > entry->size)
                size = entry->size - offset;

            memcpy(buf, entry->mapped_data + offset, size);
        } else {
            size = 0;
        }
        return size;
    }

    // Otherwise read from disk
    int fd = open(fullpath, O_RDONLY);
    if (fd == -1)
        return -errno;

    int res = pread(fd, buf, size, offset);
    close(fd);

    if (res == -1)
        res = -errno;

    return res;
}


// operations struct
static struct fuse_operations ops = {
    .getattr = my_getattr,
    .open = my_open,
    .read = my_read,
};


// main
int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &ops, NULL);
}
