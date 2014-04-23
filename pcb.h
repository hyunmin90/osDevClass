#ifndef _PCB_H
#define _PCB_H

#include "file_system.h"
#include "paging.h"

#define MAX_COMMAND_LENGTH 128
#define MAX_NUM_PROCESS 6
#define PHYSICAL_MEM_8MB 0x800000
#define KERNEL_STACK_SIZE 0x2000
#define FILE_ARRAY_SIZE 8

#define RTC_FILE_OPS_IDX 0
#define DIR_FILE_OPS_IDX 1
#define REG_FILE_OPS_IDX 2
#define STDIN_FILE_OPS_IDX 3
#define STDOUT_FILE_OPS_IDX 4
#define FILE_OPS_PTRS_SIZE 5

typedef struct file_ops_t {
  int32_t (*open)();
  int32_t (*read)();
  int32_t (*write)();
  int32_t (*close)();
} file_ops_t;

typedef struct file_desc_t {
  file_ops_t* file_ops;
  inode_t* inode_ptr;
  uint32_t file_position;
  uint32_t flags;
} file_desc_t;

typedef struct pcb_t {
  uint32_t pid;
  file_desc_t file_array[FILE_ARRAY_SIZE];
  struct pcb_t* parent_pcb;        /* pointer to parent pcb */
  pde_t* pg_dir;            /* pointer to page directory */
  int8_t cmd_name[MAX_COMMAND_LENGTH];
  int8_t cmd_args[MAX_COMMAND_LENGTH];

  uint32_t esp0;
  uint32_t ss0;
  uint32_t ebp;
} pcb_t;

pcb_t* get_new_pcb_ptr();
pcb_t* get_pcb_ptr();
pcb_t* get_global_pcb();

int32_t get_proc_index(pcb_t* pcb_ptr);
int32_t init_pcb(pcb_t* new_pcb_ptr);
int32_t destroy_pcb_ptr(pcb_t* pcb_ptr);

int32_t find_free_fd_index(pcb_t* pcb);
uint32_t fill_fd_entry(pcb_t* pcb, const uint8_t* filename, uint32_t filetype, uint32_t fd); 

#endif
