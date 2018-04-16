**Memory Resident File System**

The main header file is named myfs.h

A complete file system with up-to double indirect block addressing supporting the following functionalities:
- copy - pc to mrfs
- copy - mrfs to pc
- rm , showFile, ls, mkdir, chdir, rmdir, 
- opening, closing and reading files
- saving and restoring the entire mrfs to/from disk

The following high-level functions are provided by the header file and are used in the test files:
```cpp
int open_myfs(const char *file_name,char mode);  // TO OPEN FILES
int close_myfs(int fd);
int read_myfs (int fd, int nbytes, char *buff);
int write_myfs(int fd,int nbytes, char *buff);  // TO WRITE TO A FILE ON THE MRFS
int ls_myfs();                                  // SIMILAR TO LINUX ls command
int chdir_myfs(const char *dirname);            // SIMILAR TO LINUX cd command
int mkdir_myfs(char name[30]);                  // TO MAKE NEW DIR

// FUNCTIONS TO COPY TO AND FROM PC
int copy_myfs2pc(const char*source, const char*dest);
int copy_pc2myfs (const char *source,const char *dest);
```

