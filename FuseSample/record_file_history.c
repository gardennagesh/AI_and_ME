/* Just thought of this is simple FUSE FS for Auditing the file */
/* Whenever a time stamps of a file altered it logs basic attributes of file */
/* It maintains another file of same name suffixed with .hist in record_hist dir*/
/* Compile: gcc histfs.c -o histfs `pkg-config fuse3 --cflags --libs` */
/* Usage: mkdir record_hist; mkdir mntpoint; ./histfs mntpoint; cd mntpoint; touch file.txt; echo "edit1" > file.txt; echo "edit2" >> file.txt */
/* Observe at the content of file.txt.hist for event logs. An Daemon service is usefull to monitor specific or important files and take action accordingly*/

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <time.h>
#include <sys/stat.h>
#include <limits.h>

static const char *rootdir = "./record_hist";

/* Utility: get username */
const char* get_username(uid_t uid) {
    struct passwd *pw = getpwuid(uid);
    return pw ? pw->pw_name : "unknown";
}

/* Utility: build full path */
void fullpath(char fpath[PATH_MAX], const char *path) {
    snprintf(fpath, PATH_MAX, "%s%s", rootdir, path);
}

/* Logging function */
void log_event(const char *path, const char *op) {
    char real[PATH_MAX];
    char hist[PATH_MAX];

    fullpath(real, path);
    snprintf(hist, PATH_MAX, "%s%s.hist", rootdir, path);

    struct stat st;
    stat(real, &st);

    FILE *fp = fopen(hist, "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    struct fuse_context *ctx = fuse_get_context();
    const char *user = get_username(ctx->uid);

    fprintf(fp,
            "[TIME::%d--%d--%d %d:%d:%d] user=%s op=%s size=%ld\n",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec,
            user, op, st.st_size);

    fclose(fp);
}

/* getattr */
static int hist_getattr(const char *path, struct stat *stbuf,
                        struct fuse_file_info *fi) {
    char fpath[PATH_MAX];
    fullpath(fpath, path);
    int res = lstat(fpath, stbuf);

    if (res == -1)
        return -errno;
    return 0;
}

/* readdir */
static int hist_readdir(const char *path, void *buf,
                        fuse_fill_dir_t filler,
                        off_t offset,
                        struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags) {
    DIR *dp;
    struct dirent *de;
    char fpath[PATH_MAX];

    fullpath(fpath, path);

    dp = opendir(fpath);
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL) {
        filler(buf, de->d_name, NULL, 0, 0);
    }

    closedir(dp);
    return 0;
}

/* open (access log) */
static int hist_open(const char *path, struct fuse_file_info *fi) {
    char fpath[PATH_MAX];
    fullpath(fpath, path);

    int fd = open(fpath, fi->flags);
    if (fd == -1)
        return -errno;

    close(fd);

    log_event(path, "ACCESS");
    return 0;
}

/* read */
static int hist_read(const char *path, char *buf,
                     size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    char fpath[PATH_MAX];
    fullpath(fpath, path);

    int fd = open(fpath, O_RDONLY);
    if (fd == -1)
        return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);

    log_event(path, "ACCESS");
    return res;
}

/* write (modify log) */
static int hist_write(const char *path, const char *buf,
                      size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    char fpath[PATH_MAX];
    fullpath(fpath, path);

    int fd = open(fpath, O_WRONLY);
    if (fd == -1)
        return -errno;

    int res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);

    log_event(path, "MODIFY");
    return res;
}

/* create */
static int hist_create(const char *path, mode_t mode,
                       struct fuse_file_info *fi) {
    char fpath[PATH_MAX];
    fullpath(fpath, path);

    int fd = creat(fpath, mode);
    if (fd == -1)
        return -errno;

    close(fd);

    log_event(path, "CREATE");
    return 0;
}

/* operations */
static struct fuse_operations hist_oper = {
    .getattr = hist_getattr,
    .readdir = hist_readdir,
    .open    = hist_open,
    .read    = hist_read,
    .write   = hist_write,
    .create  = hist_create,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &hist_oper, NULL);
}
