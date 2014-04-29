#include "pcb.h"
#include "lib.h"
#include "file_system.h"
#include "rtc.h"
#include "keyboard.h"
#include "debug.h"

static const pcb_t empty_pcb;
static const file_desc_t empty_file_desc;

pcb_t* global_pcb_ptrs[MAX_NUM_PROCESS];

/* 0: RTC, 1: Directory, 2: Regular file, 3: stdin, 4: stdout */
file_ops_t file_ops_ptrs[] = {
    {rtc_open, rtc_read, rtc_write, rtc_close},
    {open_dir, read_dir_wrapper, write_dir, close_dir},
    {open_file, read_file_wrapper, write_file, close_file},
    {NULL, terminal_read, NULL, NULL},
    {NULL, NULL, terminal_write, NULL}
};

/*get_new_pcb_ptr()
  Find a space for a new PCB block, and return(if found) pointer of the
  new pcb block.
  The call to the function fails if there is no more space available for new PCB.
  The OS only supports at most MAX_NUM_PROCESS pcbs.
  Input : None
  Output : pointer to the new PCB block
           NULL if PCB could not be allocated
  Side Effects : Change global_pcb_ptrs, by updating pointer to newly allocated PCB
 */
pcb_t* get_new_pcb_ptr() {
    int i;
    for (i = 0; i < MAX_NUM_PROCESS; i++) {
        /* Find first empty pcb and claim it. 
           TBD Synchronize*/
        if (global_pcb_ptrs[i] == NULL) {
            global_pcb_ptrs[i] = (pcb_t *)(PHYSICAL_MEM_8MB - KERNEL_STACK_SIZE * (i + 2));
            return global_pcb_ptrs[i];
        }
    }
    LOG("No space is available for a new PCB.\n");
    return NULL;
}

/*destroy_pcb_ptr()
  Clean up a pcb pointed by given pcb pointer.
  Cleaned up pcb can be used by other processes in future
  Input : pcb_ptr - pointer to PCB block to be cleaned up
  Output : 0 on success
           -1 on failure, if pcb_ptr is invalid
  Side Effects : Change global_pcb_ptrs, by unbinding(Nullify) destroyed PCB's pointer
 */
int32_t destroy_pcb_ptr(pcb_t* pcb_ptr) {
    int32_t i = get_proc_index(pcb_ptr);
    if (i == -1) {
        LOG("Unable to destroy PCB becuase no matching PCB was found.\n");
        return -1;
    }
    *(global_pcb_ptrs[i]) = empty_pcb;
    global_pcb_ptrs[i] = NULL;
    return 0;
}

int32_t destroy_fd(pcb_t* pcb_ptr, int32_t fd){
  pcb_ptr -> file_array[fd] = empty_file_desc;
  return 0;
}
/*get_pcb_ptr()
  Get current PCB's pointer calculated by manipulating bits of current ESP
  Output : Current kernel stack's PCB's pointer
 */
pcb_t* get_pcb_ptr() {
    uint32_t ptr;
    asm volatile("movl %%esp, %0": "=b"(ptr));
    ptr = ptr & ~0x1FFF;
    return (pcb_t *) ptr;
}

/*get_global_pcb()
  Get the initial PCB's pointer which does not belonging to any process, 
  which is used as parent process's PCB of the first process(shell)
 */
pcb_t* get_global_pcb(){
    return (pcb_t *)(PHYSICAL_MEM_8MB - KERNEL_STACK_SIZE);
}

/*get_proc_index()
  Given pointer to PCB pointer, returns the index that corresponds to
  index of this pointer in global_pcb_ptrs.
  This index is used for statically assigning PCB, physical pages, page directories
  for different processes
  Output : Index of given pcb_ptr in global_pcb_ptrs
           -1 if not found
 */
int32_t get_proc_index(pcb_t* pcb_ptr) {
    int i;
    for (i = 0; i < MAX_NUM_PROCESS; i++) {
        if (global_pcb_ptrs[i] == pcb_ptr) {
            return i;
        }
    }
    /* No match */
    return -1;
}

/*init_pcb()
  Initialize newly created PCB block
  Setup file descriptor elements for stdin and stdout
  Input : new_pcb_ptr - pointer to PCB that needs to be initialized
  Output : 0
 */
int32_t init_pcb(pcb_t* new_pcb_ptr) {
    /*stdin*/ 
    (new_pcb_ptr->file_array)[0].file_ops = &file_ops_ptrs[STDIN_FILE_OPS_IDX]; 
    (new_pcb_ptr->file_array)[0].flags = 1;

    /*stdout*/
    (new_pcb_ptr->file_array)[1].file_ops = &file_ops_ptrs[STDOUT_FILE_OPS_IDX];
    (new_pcb_ptr->file_array)[1].flags = 1;

    return 0;
}

/*find_free_fd_index()
  Given a pointer to PCB block, find the first unused file descriptor and return it
  Input : pcb - Pointer to PCB
  Output : Index of free file descriptor for PCB
           -1 if no free FD is available
 */
int32_t find_free_fd_index(pcb_t* pcb)
{
    int32_t i;
    for(i = 2; i < FILE_ARRAY_SIZE; i++){
        if((pcb -> file_array[i]).flags == 0)
            return i;
    }
    return -1;
}
