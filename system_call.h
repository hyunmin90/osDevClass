#ifndef _SYSTEM_CALL_H
#define _SYSTEM_CALL_H

#include "types.h"

typedef struct syscall_struct_t {
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t eax;
    
    uint16_t ds;
    uint16_t ds_pad;

    uint16_t es;
    uint16_t es_pad;

    uint16_t fs;
    uint16_t fs_pad;

    uint32_t irq_num;

    uint32_t error_code;

    uint32_t return_addr;
    
    uint16_t cs;
    uint16_t cs_pad;
    
    uint32_t eflags;

    uint32_t esp;

    uint16_t ss;
    uint16_t ss_pad;
} syscall_struct_t;

extern int32_t sys_restart_syscall();

extern int32_t sys_halt(uint8_t status);

extern int32_t sys_execute(syscall_struct_t syscall_struct);

extern int32_t sys_read(int32_t fd, void* buf, int32_t nbytes);

extern int32_t sys_write(int32_t fd, const void* buf, int32_t nbytes);

extern int32_t sys_open (const uint8_t* filename);

extern int32_t sys_close (int32_t fd);

extern int32_t sys_getargs (uint8_t* buf, int32_t nbytes);

extern int32_t sys_vidmap(uint8_t** screen_start);

extern int32_t sys_set_handler(int32_t signum, void* handler_address);

extern int32_t sys_sigreturn(void);

#endif 
