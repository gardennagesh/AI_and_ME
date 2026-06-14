/* This is simple programme for backing up a file whenever it is being edited */
/* Must install libfuse3 development headers, kernel should support fuse or load module */
/* Compiled success:  gcc bkup_file.c -lfuse3 -o bkup_file */
/* Usage: mkdir -p /tmp/fuse_source /mnt/nfs_backup /tmp/mount_point; ./bkup_file /tmp/mount_point; touch /tmp/fuse_source/my_file.txt */
/* echo "Start File bkup" > /tmp/mount_point/my_file.txt */
/* cat /mnt/nfs_backup/my_file.txt */

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

//You can have multiple list of file 
#define TARGET_FILE "/my_file.txt"

// Configuration paths
static const char *source_dir = "/tmp/fuse_source"; 
static const char *nfs_dir    = "/mnt/nfs_backup";   

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int is_written = 0;

// Background thread to sync data to NFS every second
void* backup_worker(void* arg) {
   (void)arg;
   char src_path[512];
   char nfs_path[512];
   
   snprintf(src_path, sizeof(src_path), "%s%s", source_dir, TARGET_FILE);
   snprintf(nfs_path, sizeof(nfs_path), "%s%s", nfs_dir, TARGET_FILE);

   while (1) {
       sleep(1);
       pthread_mutex_lock(&lock);
       if (is_written) {
           // Execute shell command to safely sync the file
           char cmd[1024];
           snprintf(cmd, sizeof(cmd), "cp %s %s 2>/dev/null", src_path, nfs_path);
           int ret = system(cmd);
           if (ret == 0) {
               is_written = 0; 
           }
       }
       pthread_mutex_unlock(&lock);
   }
   return NULL;
}

static int backup_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
   (void)fi;
   char full_path[512];
   snprintf(full_path, sizeof(full_path), "%s%s", source_dir, path);
   
   int res = lstat(full_path, stbuf);
   if (res == -1) return -errno;
   return 0;
}

static int backup_open(const char *path, struct fuse_file_info *fi) {
   char full_path[512];
   snprintf(full_path, sizeof(full_path), "%s%s", source_dir, path);
   
   int fd = open(full_path, fi->flags);
   if (fd == -1) return -errno;
   
   fi->fh = fd;
   return 0;
}

static int backup_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   (void)path;
   int res = pread(fi->fh, buf, size, offset);
   if (res == -1) return -errno;
   return res;
}

static int backup_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
   int res = pwrite(fi->fh, buf, size, offset);
   if (res == -1) return -errno;

   // Flag file as written for backup if it matches target filename
   if (strcmp(path, TARGET_FILE) == 0) {
       pthread_mutex_lock(&lock);
       is_written = 1;
       pthread_mutex_unlock(&lock);
   }
   return res;
}

static int backup_release(const char *path, struct fuse_file_info *fi) {
   (void)path;
   close(fi->fh);
   return 0;
}

static int backup_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
   (void)offset; (void)fi; (void)flags;
   filler(buf, ".", NULL, 0, 0);
   filler(buf, "..", NULL, 0, 0);
   if (strcmp(path, "/") == 0) {
       filler(buf, "my_file.txt", NULL, 0, 0);
   }
   return 0;
}

static const struct fuse_operations backup_oper = {
   .getattr = backup_getattr,
   .open    = backup_open,
   .read    = backup_read,
   .write   = backup_write,
   .release = backup_release,
   .readdir = backup_readdir,
};

int main(int argc, char *argv[]) {
   // Setup background replication worker
   pthread_t thread_id;
   if (pthread_create(&thread_id, NULL, backup_worker, NULL) != 0) {
       return 1;
   }
   pthread_detach(thread_id);

   return fuse_main(argc, argv, &backup_oper, NULL);
}
