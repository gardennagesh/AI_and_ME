/* A programme to use shared mem to be as mutex lock btw multiple process */
/* Compile: gcc shm_mem_mutex.c -lrt -o my_mutex */
/* Usage: sudo su. In first terminal ./my_mutex procXYZ and second terminal ./my_mutex procABC */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>

#define SHM_NAME "/shared_mem_mutex"

struct shm_lock_and_data {
   pthread_mutex_t lock;
   int common_data; 
};

int main(int argc, char *argv[]) {
   int shm_fd;
   struct shm_lock_and_data *ptr_to_shm;
   int creator_of_shm = 0;

   if (argc < 2) {
       fprintf(stderr, "Usage: ./a.out  process_name\n");
       exit(1);
   }
   char *proc_id = argv[1];

   //Create shm.
   shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
   
   if (shm_fd >= 0) {
       creator_of_shm = 1;
       printf("%s Created new shared memory segment.\n", proc_id);
   } else if (errno == EEXIST) {
       // Shm already exists
       shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
       if (shm_fd < 0) {
           perror("shm_open failed");
           exit(1);
       }
   } else {
       perror("shm_open failed");
       exit(1);
   }

   // We want shm size as shm_loc_and_data.
   if (ftruncate(shm_fd, sizeof(struct shm_lock_and_data)) == -1) {
       perror("ftruncate failed");
       exit(1);
   }

   // mmap the shared memory this process. Attaching shm.
   ptr_to_shm = mmap(NULL, sizeof(struct shm_lock_and_data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
   if (ptr_to_shm == MAP_FAILED) {
       perror("mmap failed");
       exit(1);
   }

   // This Mutex must be made PTHREAD_PROCESS_SHARED by SHM Creator to use mutex among multiple process. 
   if (creator_of_shm) {
       pthread_mutexattr_t attr;
       
       pthread_mutexattr_init(&attr);
       pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
       
       pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);

       if (pthread_mutex_init(&(ptr_to_shm->lock), &attr) != 0) {
           perror("pthread_mutex_init failed");
           exit(1);
       }
       pthread_mutexattr_destroy(&attr);
       ptr_to_shm->common_data = 0;
       printf("%s Mutex initialized successfully.\n", proc_id);
   } else {
    // Sleep incase of concurrency for set lock attr. 
       sleep(1); 
   }

   for (int i = 0; i < 10; i++) {

       printf("%s Attempting to lock...\n", proc_id);
       int rc = pthread_mutex_lock(&(ptr_to_shm->lock));
       
       if (rc == EOWNERDEAD) {
           printf("%s Lock consistent as previous owner crashed.\n", proc_id);
           pthread_mutex_consistent(&(ptr_to_shm->lock));
       } else if (rc != 0) {
           perror("Lock failed");
           break;
       }

       printf("%s aquired lock. Entered critical section.\n", proc_id);
       ptr_to_shm->common_data++;
       printf("%s worked on common data %d\n", proc_id, ptr_to_shm->common_data);
       
       sleep(2); 

       printf("%s Releasing lock.\n", proc_id);
       pthread_mutex_unlock(&(ptr_to_shm->lock));
       
       // Sleep to give NICE to the other process.
       sleep(1); 
   }

   munmap(ptr_to_shm, sizeof(struct shm_lock_and_data));
   close(shm_fd);
   
   printf("%s exited gracefully.\n", proc_id);
   return 0;
}
