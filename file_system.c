/* file_system.c - Implements Read-only file system driver
 */
#include "file_system.h"
#include "lib.h"
#include "paging.h"
#include "pcb.h"

fs_stat_t fs_stat;
dentry_t* dentries;
inode_t* inodes;
data_block_t* data_blocks;

/* init_file_system()
   Parse the file system using the starting address of the physical memory in file system
   Also enable the paging for file system data
   Input : start_addr -- The physical address of the beginning of the file system
           end_addr -- The physical address of the end of the file system
   Output : None
   Side Effects : Set up global variables fs_stat, dentries, inodes, data_blocks
                  by parsing the file system according to its protocol
 */
void init_file_system(uint32_t start_addr, uint32_t end_addr) {
    fs_stat = *FS_STAT_ADDR(start_addr);
    dentries = DENTRIES_ADDR(start_addr);
    inodes = INODES_ADDR(start_addr);
    data_blocks = DATA_BLOCKS_ADDR(start_addr, fs_stat.num_inodes);
    
    /* Enable file system image in the page directory & page table */
    enable_global_pages(start_addr, end_addr);
}

/* read_dentry_by_name()
   Find a directory entry with the given name in the file system and fills in the
   directory entry pointed by dentry parameter
   Input : fname -- name of the file to be searched
           dentry -- pointer to the directory entry to be filled in
   Output : 0 on success
            -1 on fail(invalid fname, dentry, or directory entry not found)
   Side Effects : Fills in the directory entry pointed by dentry
 */
int32_t read_dentry_by_name(const uint8_t* fname, dentry_t* dentry) {
    int i;

    if (fname == NULL || dentry == NULL) {
        /* Fail if either parameter is null pointer */
        return -1;
    } else {
        uint32_t fname_length = strlen((int8_t*) fname);
        if (fname_length > MAX_FILE_NAME_LENGTH) {
            /* Given file name is too long to even compare.
               We do this to avoid reading into file_type value as well
               in case fname is too long.
             */
            return -1;
        } else {
            for(i = 0; i < fs_stat.num_dir_entries; i++) {
                /* Compare given fname with current dentry's file_name */
                if (strncmp((int8_t*) fname, (int8_t*) dentries[i].file_name, fname_length) == 0) {
                    /* At this point, j is already incremented */
                    if (fname_length < MAX_FILE_NAME_LENGTH && dentries[i].file_name[fname_length] != 0) {
                        /* It means dentry's file_name is longer than given file_name */
                        continue;
                    } else {
                        /* If we reach here, matching file name found */
                        memcpy(dentry, &(dentries[i]), DENTRY_SIZE);
                        return 0;
                    }
                } else {
                    continue;
                    /* Otherwise continue searching for next dentry */
                }
            }    
            /* Not found */
            return -1;            
        }
    }
}

/* read_dentry_by_name()
   Find a directory entry with the given directory entry index in the file system 
   and fills in the directory entry pointed by dentry parameter
   Input : index -- index of directory entry to be searched
           dentry -- pointer to the directory entry to be filled in
   Output : 0 on success
            -1 on fail(invalid index, dentry, or directory entry not found)
   Side Effects : Fills in the directory entry pointed by dentry
*/
int32_t read_dentry_by_index(uint32_t index, dentry_t* dentry) {
    if (index >= fs_stat.num_dir_entries || dentry == NULL) {
        return -1;
    } else {
        memcpy(dentry, &(dentries[index]), DENTRY_SIZE);
        return 0;
    }
}

/* read_data()
   Given the inode index, read the file's data starting from offset location, and at most
   length bytes, into buffer array
   Input : inode -- index of inode of the file to be read
           offset -- the starting position inside the file to begin reading
           buf -- array to be filled in with read data
           length -- the maximum number of bytes to be read
   Output : Number of read bytes
            -1 on failure(invalid buf pointer, inode index range, 
            and invalid block number accessed outside of range)
   Side Effects : buf array filled with copied data from the file
 */
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length) {
    if (buf == NULL || inode >= fs_stat.num_inodes) {
        /* Null check on buf, and range check on inode index */
        return -1;
    }
    /* Initialize number of bytes read to 0 */
    int32_t bytes_read = 0;

    inode_t* my_inode = &inodes[inode];
    /* Total number of bytes that can be read from the file */
    int32_t my_inode_remaining_bytes = my_inode->length - offset;

    /* Initialize current block's position in inode using offset */
    uint32_t cur_block_num = offset / BLOCK_SIZE;
    /* For first block to be copied, offset might be nonzero */
    uint32_t cur_block_offset = offset % BLOCK_SIZE;

    /* Loop while remaining bytes that needs to be read is greater than 0, 
       remaining bytes in file is greater than 0, 
    */
    while (length > 0 && my_inode_remaining_bytes > 0) {
        /* current index of block in file system */
        int32_t cur_block_index = my_inode->block_idx[cur_block_num];
        if (cur_block_index >= fs_stat.num_data_blocks) {
            /* invalid block's index; Fail! */
            return -1;
        }
        data_block_t* cur_block = &data_blocks[cur_block_index];
        int32_t cur_block_remaining_bytes = BLOCK_SIZE - cur_block_offset;
        
        /* Calculate number of bytes to be copied */
        int32_t num_bytes_to_copy;
        if (length >= cur_block_remaining_bytes) {/* If data to be read is exceeding current block */
            /* If remaining file's length is longer than current block's remaining bytes, read all
               remaining current block's remaining bytes.
               Otherwise, it means file is ending before the end of block. Read upto end of the file
            */
            num_bytes_to_copy = (my_inode_remaining_bytes >= cur_block_remaining_bytes)?
                cur_block_remaining_bytes : my_inode_remaining_bytes;
        } else {/* Otherwise number of bytes to be read does not exceed current block */
            /* If remaining file's length is longer than remaining bytes to be read, then read all
               remaining bytes to be read.
               Otherwise, it means file is ending before reading all requested bytes. Read upto end of the file.
            */
            num_bytes_to_copy = (my_inode_remaining_bytes >= length)? length : my_inode_remaining_bytes;
        }

        /* Copy bytes */
        memcpy((buf + bytes_read), ((uint8_t *)cur_block + cur_block_offset), num_bytes_to_copy);
        /* Decrement number of bytes left in file */
        my_inode_remaining_bytes -= num_bytes_to_copy;
        /* Decrement number of bytes to be read */
        length -= num_bytes_to_copy;
        /* Increment number of bytes read so far*/
        bytes_read += num_bytes_to_copy;

        /* After first block, blocks should be copied from their starting positions */
        cur_block_offset = 0;
        /* Traverse next block in inode */
        cur_block_num++;
    }
    return bytes_read;
}

/* open_file()
   Open a file with the given file name
   Input : fname -- file name string
   Output : address of the inode that corresponds to the searched file
            -1 if the file with the given name does not exists, or fname is NULL
   Side Effects : None
 */
int32_t open_file(const uint8_t* fname) {
    dentry_t dentry;
    if (fname == NULL || read_dentry_by_name(fname, &dentry) == -1) {
        return -1;
    } else {
        return (int32_t) &inodes[dentry.inode_idx];
    }
}

/* read_file()
   Given the inode's pointer, read the file's data starting from offset location, and at most
   length bytes, into buffer array
   Input : inode_ptr -- pointer to the inode of the file to be read
           offset -- the starting position inside the file to begin reading
           buf -- array to be filled in with read data
           length -- the maximum number of bytes to be read
   Output : Number of read bytes,
            -1 on failure(invalid buf pointer, NULL inode pointer,
             and invalid block number accessed outside of range)
   Side Effects : buf array filled with copied data from the file
*/
int32_t read_file(inode_t* inode_ptr, uint32_t offset, uint8_t* buf, uint32_t length) {
    if (inode_ptr == NULL) {
        return -1;
    } else {
        int i;
        /* Iterate through inodes and find a inode that points to the same inode as the input inode */
        for (i = 0; i < fs_stat.num_inodes; i++) {
            if (&inodes[i] == inode_ptr) {
                return read_data(i,  offset, buf, length);
            }
        }
        /* If matching inode is not found, retrun null */
        return -1;
    }
}

int32_t read_file_wrapper(int32_t fd, uint8_t* buf, uint32_t length){
  pcb_t* pcb = get_pcb_ptr();
  inode_t* inode_ptr = (((pcb -> file_array)[fd]).inode_ptr);
  uint32_t offset = (((pcb -> file_array)[fd]).file_position);

  int32_t bytes_read = read_file(inode_ptr, offset, buf, length);
  ((pcb -> file_array)[fd]).file_position += bytes_read;
  return bytes_read;
}

/* write_file()
   Write data into a file
   It fails no matter what because the file system is read only
   Input : inode_ptr -- pointer to the inode of the file to be written
           offset -- the starting position inside the file to begin writing
           buf -- array of the data to be written
           length -- the maximum number of bytes to be written
   Output : -1 always, as it is impossible to write into the read-only file system
   Side Effects : None
*/
int32_t write_file(inode_t* inode_ptr, uint32_t offset, uint8_t* buf, uint32_t length) {
    return -1;
}

/* close_file()
   Close the file.
   It fails no matter what because the file system is read only
   Input : inode_ptr -- pointer to the inode of the file to be closed
   Output : 0 always, as there is nothing to be done to close a file
   Side Effects : None
*/
int32_t close_file(inode_t* inode_ptr) {
    return 0;
}

/* open_dir()
   Open a directory with the given file name
   Input : fname -- file name string
   Output : dir_index, representing the requested directory's directory entry index
           -1 if the directory with the given name does not exists, or fname is NULL
   Side Effects : None
 */
int32_t open_dir(const uint8_t* fname) {
    dentry_t dentry;
    if(fname == NULL || 
        read_dentry_by_name(fname, &dentry) == -1 ||
        dentry.file_type != FILE_TYPE_DIR) {
        return -1;
    } else {
        int i;
        uint32_t fname_length = strlen((int8_t*) fname);
        if (fname_length > MAX_FILE_NAME_LENGTH) {
            /* Given file name is too long to even compare.
               We do this to avoid reading into file_type value as well
               in case fname is too long.
             */
            return -1;
        } else {
            for(i = 0; i < fs_stat.num_dir_entries; i++) {
                /* Compare given fname with current dentry's file_name */
                if (strncmp((int8_t*) fname, (int8_t*) dentries[i].file_name, fname_length) == 0) {
                    /* At this point, j is already incremented */
                    if (fname_length < MAX_FILE_NAME_LENGTH && dentries[i].file_name[fname_length] != 0) {
                        /* It means dentry's file_name is longer than given file_name */
                        continue;
                    } else {
                        /* If we reach here, matching file name found */
                        return i;
                    }
                } else {
                    continue;
                    /* Otherwise continue searching for next dentry */
                }
            }    
            /* Not found */
            return -1;            
        }
    }
}

/* read_dir()
   Given the offset, read the (offset)'th directory entry's file name, at most
   length bytes, into buffer array
   Input : inode_ptr -- pointer to the inode of the file to be read
           offset -- the starting position inside the file to begin reading
           buf -- array to be filled in with file name
           length -- the maximum number of bytes to be read
   Output : Number of read bytes
   Side Effects : buf array filled with copied file name from the file
*/
int32_t read_dir(inode_t* inode_ptr, uint32_t offset, uint8_t* buf, uint32_t length) {
    int i;
    uint32_t file_name_length = 0;

    if (fs_stat.num_dir_entries <= offset) {
        return 0;
    } else {
        /* obtain the length of the name of file */
        for (i = 0; i < MAX_FILE_NAME_LENGTH; i++) {
            if (dentries[offset].file_name[i] != NULL) {
                file_name_length++;
            }
            else {
                break;
            } 
        }
        /* if the file name length is greater than the given length, cut it out */
        // if(file_name_length >= length) file_name_length = length;
        file_name_length = min(file_name_length, length);
       
        /* copy the file name into the buffer array */
        strncpy((int8_t*)buf, (int8_t*)dentries[offset].file_name, file_name_length);
        
        return file_name_length;
    }
}

int32_t read_dir_wrapper(int32_t fd, uint8_t* buf, uint32_t length){
  pcb_t* pcb = get_pcb_ptr();
  inode_t* inode_ptr = (((pcb -> file_array)[fd]).inode_ptr);
  uint32_t offset = (((pcb -> file_array)[fd]).file_position);

  int32_t bytes_read = read_dir(inode_ptr, offset, buf, length);
  if (bytes_read > 0) {
      ((pcb -> file_array)[fd]).file_position += 1;
  }
  return bytes_read;
}

/* write_dir()
   Write filename into a directory
   It fails no matter what because the file system is read only
   Input : inode_ptr -- pointer to the inode of the file to be written
           offset -- the starting position inside the file to begin writing
           buf -- array of the data to be written
           length -- the maximum number of bytes to be written
   Output : -1 always, as it is impossible to write into the read-only file system
   Side Effects : None
*/
int32_t write_dir(inode_t* inode_ptr, uint32_t offset, uint8_t* buf, uint32_t length) {
    return -1;
}

/* close_dir()
   Close the directory.
   It fails no matter what because the file system is read only
   Input : inode_ptr -- pointer to the inode of the file to be closed
   Output : 0 always, as there is nothing to be done to close a directory
   Side Effects : None
*/
int32_t close_dir(inode_t* inode_ptr) {
    return 0;
}

/* test_file_system_driver()
   Prints out the first few bytes of the regular files in the file system, along with their sizes
   This is not a real funcfion that does any real functionality
   It is intended to be tested in gdb
*/
void test_file_system_driver(void) {
    const uint32_t buf_size = BLOCK_SIZE * 10;
    uint8_t buf[buf_size];
    const uint32_t file_name_length = MAX_FILE_NAME_LENGTH;
    uint8_t buf2[MAX_FILE_NAME_LENGTH];
    int32_t temp;
    int i;

    /* this is for testing directory-related functions */
    // dentries[3].file_type = FILE_TYPE_DIR;
    // dentries[5].file_type = FILE_TYPE_DIR;
    // dentries[8].file_type = FILE_TYPE_DIR;
    // dentries[9].file_type = FILE_TYPE_DIR;

    for (i = 0; i < 16; i++) {
        dentry_t my_dentry;
        int result = read_dentry_by_index(i, &my_dentry);
        if (result != 0) {
            printf("Error while reading dentry at index %d\n", i);
        } else {
            if (my_dentry.file_type == FILE_TYPE_FILE) {
                inode_t my_inode = inodes[my_dentry.inode_idx];
                printf("file name : %s, file type : regular, expected file size : %d bytes\n", my_dentry.file_name, my_inode.length);
                int j;
                for (j = 0; j < buf_size; j++) {
                    // clear out buffer
                    buf[j] = NULL;
                }
                /* Test open file */
                inode_t *my_inode_ptr = (inode_t *)open_file(my_dentry.file_name);
                /* Test read_file */
                result = read_file(my_inode_ptr, 0, buf, buf_size);
                printf("Number of read bytes : %d\n", result);
                printf("First 80 bytes : \n");
                for (j = 0; j < min(result, 80); j++) {
                    printf("%c", buf[j]);
                }
                printf("\nend of first 80 bytes for file %s\n", my_dentry.file_name);
            } else if (my_dentry.file_type == FILE_TYPE_DIR) {
                temp = open_dir(my_dentry.file_name);
                printf("index number for directory: %d\n", temp);
                  
                result = read_dir(0, temp, buf2, file_name_length);
                
                printf("Number of read bytes for directory: %d\n", result);
                int j;
                for (j = 0; j < min(result, file_name_length); j++) {
                  printf("%c", buf2[j]);
                }
                printf("file name : %s, file type : directory\n", my_dentry.file_name);
            } else {
                printf("file name : %s, file type : rtc\n", my_dentry.file_name);
            }
        }
    }
}
