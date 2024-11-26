#include "common.h"
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


/* make sure to use syserror() when a system call fails. see common.h */

void copy_file(char* src_directory, char* dest_directory, mode_t mode){
    
    // Create a file descriptor to read source file
    int readFD = open(src_directory, O_RDONLY);
    if(readFD < 0){syserror(open, src_directory);}
    
    // Create a file descriptor to write destination file
    int writeFD = creat(dest_directory, S_IWUSR);
    if(writeFD < 0){syserror(open, dest_directory);}
    
    // Create a buffer that stores bytes read from source file
    char buf[4096];
    int readAmount = read(readFD, buf, 4096);
    if(readAmount < 0){syserror(read, src_directory);}
    
    int writeAmount;
    
    // Keeping writing bytes to destination file until all bytes in source file are read
    while(readAmount != 0){
        writeAmount = write(writeFD, buf, readAmount);
        if(writeAmount < 0){syserror(write, dest_directory);}
        
        readAmount = read(readFD, buf, 4096);
        if(readAmount < 0){syserror(read, src_directory);}
    }
    
    //Close both file descriptor
    int closeRdReturn = close(readFD);
    if(closeRdReturn < 0){syserror(close, src_directory);}
    
    int closeWrReturn = close(writeFD);
    if(closeWrReturn < 0){syserror(close, dest_directory);}
    
    // Set the mode of destination file the same as source file
    int chmodFileReturn = chmod(dest_directory, mode);
    if(chmodFileReturn < 0){syserror(chmod, dest_directory);}
}

void copy_directory(char* src_directory, char* dest_directory){
    
    // Create destination directory
    int mkdirReturn = mkdir(dest_directory, 0777);
    if(mkdirReturn < 0){syserror(mkdir, dest_directory);}
    
    // Create stream for items in the directory
    DIR* dirStream = opendir(src_directory);
    
    // Parse stream
    while(dirStream){
        struct dirent* item = readdir(dirStream);
        
        if (item == NULL){
            closedir(dirStream);
            break;
        }
       
        // Create new source directory path
        char newSrcDir[256];
        strcpy(newSrcDir, src_directory);
        strcat(newSrcDir, "/");
        strcat(newSrcDir, item -> d_name);
        
        // Create new destination directory path
        char newDestDir[256];
        strcpy(newDestDir, dest_directory);
        strcat(newDestDir, "/");
        strcat(newDestDir, item -> d_name);
        
        // Get the status of the item
        struct stat itemStatBuf;
        int itemStatReturn = stat(newSrcDir, &itemStatBuf);
        if(itemStatReturn < 0){syserror(stat, newSrcDir);}
        
        // Copy the item to the destination directory if it is a file
        if(S_ISREG(itemStatBuf.st_mode)) {
            copy_file(newSrcDir, newDestDir, itemStatBuf.st_mode);
        }
        
        // Do recursive call to the item if it is a directory and make sure it's not current or parent directory
        // to avoid infinite recursion. Takes me two days to realize this :)
        else if(S_ISDIR(itemStatBuf.st_mode) && strcmp(item -> d_name, ".") != 0 && strcmp(item -> d_name, "..") != 0) {
            copy_directory(newSrcDir, newDestDir);
        }
    }
    
    // Get the status of the source directory
    struct stat srcStatBuff;
    int srcStatReturn = stat(src_directory, &srcStatBuff);
    if(srcStatReturn < 0){syserror(stat, src_directory);}
    
    // Set the mode of the destination directory to the same as of source
    int chmodDirReturn = chmod(dest_directory, srcStatBuff.st_mode);
    if(chmodDirReturn < 0){syserror(chmod, dest_directory);}
}

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	if (argc != 3) {
		usage();
	}
        
        copy_directory(argv[1], argv[2]);
	//TBD();
	return 0;
}