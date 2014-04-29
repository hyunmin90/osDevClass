/* file_system.h - Declare Read-only file system driver's header along with 
                   useful constants and macros
 */
#ifndef _FILE_SYSTEM_H
#define _FILE_SYSTEM_H

#include "types.h"

#define FILE_TYPE_RTC 0
#define FILE_TYPE_DIR 1
#define FILE_TYPE_FILE 2

#define DENTRY_SIZE 64
#define BLOCK_SIZE 4096
#define MAX_NUM_BLOCKS_IN_INODE 1023

#define MAX_FILE_NAME_LENGTH 32
#define FILE_HEADER_SIZE 40
#define RESERVED_BYTE_BOOT 13
#define RESERVED_BYTE_DENTRY 6

/* File system's statistics information */
typedef struct fs_stat_t {
    uint32_t num_dir_entries;
    uint32_t num_inodes;
    uint32_t num_data_blocks;
    uint32_t reserved[RESERVED_BYTE_BOOT];
} fs_stat_t;

/* A single directory entry */
typedef struct dentry_t {
    uint8_t file_name[MAX_FILE_NAME_LENGTH];
    uint32_t file_type;
    uint32_t inode_idx;
    uint32_t reserved[RESERVED_BYTE_DENTRY];
} dentry_t;

/* An inode that contains number of bytes in file and blocks' indexes */
typedef struct inode_t {
    uint32_t length;
    uint32_t block_idx[MAX_NUM_BLOCKS_IN_INODE];
} inode_t;

/* A single data block of size 4kB */
typedef struct data_block_t {
    uint8_t data[BLOCK_SIZE];
} data_block_t;

/* 40 bytes long file header that identifies executable files */
typedef struct file_header_t {
    union {
        uint8_t data[FILE_HEADER_SIZE];
        struct {
            uint8_t elf[4];
            uint8_t pad1[20];
            int32_t* entry_ptr;
            uint8_t pad2[12];
        } __attribute__((packed));
    };
} file_header_t;

void init_file_system(uint32_t start_addr, uint32_t end_addr);
int32_t read_dentry_by_name(const uint8_t* fname, dentry_t* dentry);
int32_t read_dentry_by_index(uint32_t index, dentry_t* dentry);
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length);

/* Open, read, write, close functions on files for system call functions support */
int32_t open_file(const uint8_t* filename);
int32_t read_file(inode_t* inode_ptr, uint32_t offset, uint8_t* buf, uint32_t length);
int32_t write_file(inode_t* inode_ptr, uint32_t offset, uint8_t* buf, uint32_t length);
int32_t close_file(inode_t* inode_ptr);

int32_t read_file_wrapper(int32_t fd, uint8_t* buf, uint32_t length);
int32_t read_dir_wrapper(int32_t fd, uint8_t* buf, uint32_t length);

/* Open, read, write, close functions on directories for system call functions support */
int32_t open_dir(const uint8_t* filename);
int32_t read_dir(inode_t* inode_ptr, uint32_t offset, uint8_t* buf, uint32_t length);
int32_t write_dir(inode_t* inode_ptr, uint32_t offset, uint8_t* buf, uint32_t length);
int32_t close_dir(inode_t* inode_ptr);

void test_file_system_driver(void);

/* Macros to help parsing the file system */
#define FS_STAT_ADDR(BASE_ADDR) ((fs_stat_t*) BASE_ADDR)
#define DENTRIES_ADDR(BASE_ADDR) ((dentry_t*) (BASE_ADDR + DENTRY_SIZE))
#define INODES_ADDR(BASE_ADDR) ((inode_t*) (BASE_ADDR + BLOCK_SIZE))
#define DATA_BLOCKS_ADDR(BASE_ADDR, NUM_INODES) ((data_block_t*) (BASE_ADDR + BLOCK_SIZE * (NUM_INODES + 1)))

#endif /* _FILE_SYSTEM_H */
