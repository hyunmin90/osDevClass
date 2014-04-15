#include "pcb.h"
#include "lib.h"

pcb_t* global_pcb_ptrs[MAX_NUM_PROCESS];

pcb_t* get_new_pcb_ptr() {
    int i;
    for (i = 0; i < MAX_NUM_PROCESS; i++) {
        /* Find first empty pcb and claim it. 
           TBD Synchronize*/
        if (global_pcb_ptrs[i] == NULL) {
            global_pcb_ptrs[i] = (pcb_t *)(PHYSICAL_MEM_8MB - KERNEL_STACK_SIZE * (i + 1));
            return global_pcb_ptrs[i];
        }
    }
    LOG("No space is available for a new PCB.\n");
    return NULL;
}

pcb_t* get_pcb_ptr() {
    uint32_t ptr;
    asm volatile("movl %%esp, %0": "=b"(ptr));
    ptr = ptr & ~0x1FF;
    return (pcb_t *) ptr;
}

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

int32_t init_pcb(pcb_t* new_pcb_ptr) {
    /* TODO */
    /* Need to setup file descriptor array for stdin and stdout */
    return 0;
}