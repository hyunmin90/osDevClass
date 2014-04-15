#ifndef _PCB_H
#define _PCB_H

#include "file_system.h"
#include "paging.h"

#define MAX_COMMAND_LENGTH 128
#define MAX_NUM_PROCESS 6
#define PHYSICAL_MEM_8MB 0x7FFFFF
#define KERNEL_STACK_SIZE 0x1FFF
#define FILE_ARRAY_SIZE 8

typedef struct file_ops_t {
  uint32_t* open;
  uint32_t* read;
  uint32_t* write;
  uint32_t* close;
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
} pcb_t;

pcb_t* get_new_pcb_ptr();
pcb_t* get_pcb_ptr();
int32_t get_proc_index(pcb_t* pcb_ptr);
int32_t init_pcb(pcb_t* new_pcb_ptr);

#endif
