#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include "disk.h"
#include "fs.h"

#define FAT_MAX_ENTRIES 2048
#define OPEN_FILES_MAX 32

struct superB {
    uint8_t signature[8]; //Signature (must be equal to "ECS150FS")
    uint16_t total_blk_count; //Total amount of blocks of virtual disk
    uint16_t rdir_blk; //Root directory block index
    uint16_t data_blk; //Data block start index
    uint16_t data_blk_count; //Amount of data blocks
    uint8_t fat_blk_count; //Number of blocks for FAT
    uint8_t padding[4079];
};

struct superB sb;

struct file {
   char filename[FS_FILENAME_LEN];
   uint32_t size;
   uint16_t index;
   uint8_t padding[10];
};

struct root {
   struct file entries[FS_FILE_MAX_COUNT];
};

struct fat{
    uint16_t entries[FAT_MAX_ENTRIES];
};

struct fat fats;

struct root root;

struct fileDescriptor{
    int fd;
    int offset;
    char filename[FS_FILENAME_LEN];
};

struct fileDescriptor openFds[OPEN_FILES_MAX];

int getFD(int fd){
    for(int i = 0; i < OPEN_FILES_MAX; i++)
        if(openFds[i].fd == fd)
            return i;
    return -1;
}

int getFat(int off, int blk){
    int numBlks = off/BLOCK_SIZE;
    while (numBlks>0){
        blk = fats.entries[blk];
        numBlks--;
        if (blk == 0xFFFF)
            return -1;
    }
    return blk;
}


int getFatFree(){
                int count = 0;
    for (int i=0; i<sb.data_blk_count; i++){
        if (fats.entries[i] == 0){
                count++;
        }
    }
    return count;
}


int getRootFree(){
    int count = 0;
    for (int i=0; i<FS_FILE_MAX_COUNT; i++){
        if (root.entries[i].index == 0)
            count++;
    }
    return count;
}

int fileInRoot(const char *filename){
        for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
                if (strcmp(root.entries[i].filename, filename) == 0){
                        return 0;
                }
        }
        return -1;
}

int getOpenFree(){
    int count = 0;
    for (int i = 0; i < OPEN_FILES_MAX; i++){
        //printf("fd == %d in openFree\n", openFds[i].fd);
        if(openFds[i].fd == 0){
            //printf("fd == 0\n");
            count++;
        }
                                //else{
                                //      printf("file is at index %d in openFd[], fd is : %d\n", i, openFds[i].fd);
                                //}
    }
                return count;
}

int nextOpen(void){
                int index = 0;
                for (int i = 0; i < OPEN_FILES_MAX; i++){
                        index = i;
                        if(openFds[i].fd == 0){
                                return index;
                        }
                }
                return 0;
}

int openFile(const char *filename){
        for (int i = 0; i < OPEN_FILES_MAX; i++){
                if (strcmp(openFds[i].filename, filename) == 0){
                        return i;
                }
        }
        return -1;
}

int openFdsDescriptorCheck(int fd){
        for(int i = 0; i < OPEN_FILES_MAX; i++){
                if(openFds[i].fd == fd){
                        return 0;
                }
        }
        return -1;
}

int freeIndexFat(void){
    for (int i = 0; i<FAT_MAX_ENTRIES; i++){
        if(fats.entries[i] == 0){
            fats.entries[i] = 0xFFFF;
            return 0;
        }
    }
    return -1;
}

void printFdDir(void){
        DIR *d;
        struct dirent *dir;
        d = opendir("/proc/self/fd/");
        if (d) {
                while ((dir = readdir(d)) != NULL) {
                        //printf("%s\n", dir->d_name);
                }
                closedir(d);
        }
}


int nextFreeFat(){
    for (int i=0; i<sb.data_blk_count; i++){
        if (fats.entries[i] == 0)
            return 0;
    }
    return -1;
}

void addFats(int blk, int numBlks){
   int count;
   while (fats.entries[blk] != 0xFFFF){
       blk = fats.entries[blk];
       count++;
   }
    for (int i=0; i<numBlks-count; i++){
        fats.entries[blk] = nextFreeFat();
        blk = fats.entries[blk];
    }
}


int fs_mount(const char *diskname)
{
    if (block_disk_open(diskname)){
        perror("disk_open");
        return -1;
    }
    if (block_read(0, &sb) < 0){
        perror("sb read");
        return -1;
    }

    //fats = malloc(sb.fat_blk_count * sizeof(struct fat));

    for (int i=0; i<sb.fat_blk_count; i++){
        if (block_read(i+1, &fats) < 0){
            perror("fat read");
            return -1;
        }
    }
    if (block_read(sb.rdir_blk, &root) < 0){
        perror("root read");
        return -1;
    }
                fats.entries[0] = 0xFFFF;
    return 0;
}

int fs_umount(void){
    if (block_write(0, &sb) < 0){
        perror("sb write");
        return -1;
    }

    for (int i=0; i<sb.fat_blk_count; i++){
        if (block_write(i+1, &fats) < 0){
            perror("fat write");
            return -1;
        }
    }
    //free(fats);

    if (block_write(sb.rdir_blk, &root) < 0){
        perror("root write");
        return -1;
    }
                if(getOpenFree() < 32){
                        return -1;
                }
    if(block_disk_close() < 0){
                        return -1;
                }

    return 0;
}

int fs_info(void)
{
                if(block_disk_count() < 0){
                        return -1;
                }
    printf("FS Info:\n");
    printf("total_blk_count=%d\n", sb.total_blk_count);
    printf("fat_blk_count=%d\n", sb.fat_blk_count);
    printf("rdir_blk=%d\n", sb.rdir_blk);
    printf("data_blk=%d\n", sb.data_blk);
    printf("data_blk_count=%d\n", sb.data_blk_count);
    printf("fat_free_ratio=%d/%d\n", getFatFree(), sb.data_blk_count);
    printf("rdir_free_ratio=%d/%d\n", getRootFree(), FS_FILE_MAX_COUNT);

    return 0;
}

int fs_create(const char *filename){
    // make sure filename is not too long
    if(strlen(filename) > FS_FILENAME_LEN)
        return -1;
    
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
        // make sure no duplicate filenames
         if (strcmp(root.entries[i].filename,filename)==0)
            return -1;
        
    }
    if(getRootFree() == 0){
            return -1;
    }
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if(root.entries[i].filename[0] == '\0'){
//                                              creat(filename, 0644);
            struct file* newEntry = (struct file*)malloc(sizeof(struct file*));
            strcpy(newEntry->filename, filename);
            struct stat st;
            if(stat(filename, &st)<0)
                return -1;
            newEntry->size = st.st_size;
            if (newEntry->size == 0)
                newEntry->index = 0xffff;
            else
                newEntry->index = i+1;
            root.entries[i] = *newEntry;
                                                //freeIndexFat();
                                free(newEntry);
                        return 0;
        }
    }
    return 0;
}

int clearFat(int index){
  int check = 1;
  if(index == 0xFFFF)
    check = 0;
  int i = index;
  int stop = 0;
  while(check == 1){
    if((fats.entries[i] == 0xFFFF)){// || (i == 0)){
      if(i != 0){
        fats.entries[i] = 0;
      }
      check = 0;
      return 0;
    }
    i = fats.entries[i];
    if(i == 0){
      return 0;
    }
    fats.entries[i] = 0;
    //if(stop == 10)
    //  return -1;
    //stop++;
  }
  return -1;
}

int fs_delete(const char *filename)
{
    //int index = 0;
    // make sure filename passed is not too large
                if(filename == NULL){
                        return -1;
                }
                if(openFile(filename) >= 0){
                        return -1;
                }

    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if (strcmp(root.entries[i].filename,filename)==0){
            *root.entries[i].filename = '\0';
                // Weird behavior from this function
                                                clearFat(root.entries[i].index);
            root.entries[i].size = 0;
            root.entries[i].index = 0;
            return 0;
        }
    }
    return -1;
}


int fs_ls(void)
{
    if(block_disk_count() < 0){
      return -1;
    }
                // not sure if both work
    printf("FS Ls:\n");

    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if (root.entries[i].index != 0){
            printf("file: %s, ", root.entries[i].filename);
            printf("size: %d, ", root.entries[i].size);
            printf("data_blk: %d\n", root.entries[i].index);
        }
    }
    return 0;
}


// this fs_open is not reliant on a openFile array, if the returned file descriptor value is above (FS_OPEN_MAX + 3) then it is invalid
int fs_open(const char *filename)
{
    if(filename == NULL){
      return -1;
    }

                //printFdDir();
    if((strlen(filename) > FS_FILENAME_LEN)){
        return -1;
    }
                if(getOpenFree() == 0){
                        return -1;
                }
                if(fileInRoot(filename) != 0){
                        return -1;
                }
    int fd;
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
        // find file with name 'filename' in
        if (strcmp(root.entries[i].filename,filename)==0){
                        fd = open(filename, O_RDWR);
                                                //printf("fd in fs_open is: %d ************************\n", fd);
                                    //if((fd = open(filename, O_RDWR)) >= (OPEN_FILES_MAX+5)){
            //    printf("error, too many open files\n");
            //    return -1;
            //}
                                                //printf("open files left is: %d\n", count);
            struct fileDescriptor* newFd = (struct fileDescriptor*)malloc(sizeof(struct fileDescriptor*));
            newFd->fd = fd;
            newFd->offset = 0;
            strcpy(newFd->filename, filename);
            int index = nextOpen();
            openFds[index] = *newFd;
//
            return fd;
        }
    }
    return -1;
}

int fs_close(int fd)
{
    //if(fd >= OPEN_FILES_MAX){
    //    printf("File Descriptor is out of bounds\n");
    //    return -1;
    //}
                if(openFdsDescriptorCheck(fd) < 0){
                        return -1;
                }
                int index = 0;
    for (int i = 0; i < OPEN_FILES_MAX; i++){
                        if(openFds[i].fd == fd){
                                index = i;
                                break;
                        }
      //if(i == (OPEN_FILES_MAX - 1)){
      //  printf("File Descriptor is out of bounds\n");
      //  return -1;
      //}
                }
    openFds[index].fd = 0;
    openFds[index].offset = 0;
    char empty[FS_FILENAME_LEN];
    strcpy(openFds[index].filename, empty);

    if(close(fd) < 0){
        return -1;
    }

    return 0;
}

int fs_stat(int fd){
    if(openFdsDescriptorCheck(fd) < 0){
      return -1;
    }
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if (openFds[i].fd == fd){
            struct stat st;
            if(stat(openFds[i].filename, &st)<0)
                return -1;
            return st.st_size;
        }
    }
    return -1;
}
int fs_lseek(int fd, size_t offset){
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
         if (openFds[i].fd == fd){
            openFds[i].offset = offset;
             return 0;
         }
    }
    return -1;
}

int fs_write(int fd, void *buf, size_t count){
    if(fd >= (OPEN_FILES_MAX+5)){
        return -1;
    }
    //fs_lseek(fd, fs_stat(fd));
    int ind = getFD(fd);
    if (ind < 0)
        return -1;

    int blk = getFat(openFds[ind].offset, openFds[ind].fd);
    if (blk == -1)
        return -1;
    int newOffset = openFds[ind].offset;
    if (newOffset >= BLOCK_SIZE)
        newOffset = newOffset%BLOCK_SIZE;
    int spanBlks = (count+newOffset-1)/BLOCK_SIZE;
    addFats(openFds[ind].fd, spanBlks);

    for (int i=0; i<=spanBlks; i++){
        char bounce[BLOCK_SIZE];
        if (i==0){
            if (sizeof(buf)+newOffset <= BLOCK_SIZE)
                memcpy(bounce+newOffset, buf, count);
            else
                memcpy(bounce+newOffset, buf, BLOCK_SIZE-newOffset);
            if(block_write(blk, bounce) < 0)
                return -1;
        } else if (i==spanBlks){
            memcpy(bounce, buf+BLOCK_SIZE, count%BLOCK_SIZE);
            if(block_write(blk, bounce) < 0)
                return -1;
        } else {
            blk = fats.entries[blk];
        }
    }

    openFds[ind].offset += count;
    return count;
}

int fs_read(int fd, void *buf, size_t count){
    int ind = getFD(fd);
    if (ind < 0){
        return -1;
    }

    int blk = getFat(openFds[ind].offset, openFds[ind].fd);
    int newOffset = openFds[ind].offset;
    if (newOffset >= BLOCK_SIZE)
        newOffset = newOffset%BLOCK_SIZE;
    int spanBlks = (count+newOffset-1)/BLOCK_SIZE;

    for (int i=0; i<=spanBlks; i++){
        char bounce[BLOCK_SIZE];
        if (i==0){
            if(block_read(blk, bounce) < 0)
                return -1;
            if (count+newOffset <= BLOCK_SIZE)
                memcpy(buf, bounce+newOffset, count);
            else
                memcpy(buf, bounce+newOffset, BLOCK_SIZE-newOffset);
        } else if (i==spanBlks){
            if(block_read(blk, bounce) < 0)
                return -1;
            memcpy(buf, bounce, count%BLOCK_SIZE);
        } else {
            if(block_read(blk, bounce) < 0)
                return -1;
            memcpy(buf, buf, BLOCK_SIZE);
            blk = fats.entries[blk];
        }
    }

    openFds[ind].offset += count;
    return count;
}
