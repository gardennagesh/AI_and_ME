/* AI is fantastic tool for experienced programmers */ 
/* This is generated using AI to demo the use of FUSE */
/* A simple programme which hides .txt suffixed files */
/* This programme uses FUSE lib and FUSE API */
/* Install FUSE lib to compile this */
/* Compile: cc -Wall hide_txt_fs.c -o hide_txt_fs `pkg-config fuse --cflags --libs`*/
/* Usage: mkdir /tmp/mntpoint; ./hide_txt_fs /my_src_dir /tmp/mntpoint */

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

static char *rootdir;

/* Helper: check if file ends with .txt */
int is_txt(const char *path) {
    const char *ext = strrchr(path, '.');
    return (ext && strcmp(ext, ".txt") == 0);
}

/* Build full path */
void fullpath(char fpath[PATH_MAX], const char *path) {
    strcpy(fpath, rootdir);
    strncat(fpath, path, PATH_MAX);
}

/* getattr */
static int hide_getattr(const char *path, struct stat *stbuf) {
    char fpath[PATH_MAX];
    fullpath(fpath, path);

    if (is_txt(path)) {
        return -ENOENT;
    }

    int res = lstat(fpath, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

/* readdir */
static int hide_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi) {
    DIR *dp;
    struct dirent *de;
    char fpath[PATH_MAX];

    fullpath(fpath, path);

    dp = opendir(fpath);
    if (dp == NULL)
        return -errno;
  
    // skip .txt files
    while ((de = readdir(dp)) != NULL) {
        if (is_txt(de->d_name)) {
            continue; 
        }

        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buf, de->d_name, &st, 0))
            break;
    }

    closedir(dp);
    return 0;
}

/* open */
static int hide_open(const char *path, struct fuse_file_info *fi) {
    char fpath[PATH_MAX];
    fullpath(fpath, path);

    if (is_txt(path)) {
        return -ENOENT;
    }

    int fd = open(fpath, fi->flags);
    if (fd == -1)
        return -errno;

    close(fd);
    return 0;
}

/* read */
static int hide_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    int fd;
    int res;
    char fpath[PATH_MAX];

    fullpath(fpath, path);

    if (is_txt(path)) {
        return -ENOENT;
    }

    fd = open(fpath, O_RDONLY);
    if (fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    return res;
}

/* FUSE operations */
static struct fuse_operations hide_oper = {
    .getattr = hide_getattr,
    .readdir = hide_readdir,
    .open    = hide_open,
    .read    = hide_read,
};

/* main */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <src_dir> <mnt_point>\n", argv[0]);
        return 1;
    }

    rootdir = realpath(argv[1], NULL);

    // Shift arguments so FUSE sees only mountpoint
    argv[1] = argv[2];
    argc--;

    return fuse_main(argc, argv, &hide_oper, NULL);
}
