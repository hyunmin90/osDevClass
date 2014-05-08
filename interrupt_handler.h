#ifndef _INTERRUPT_HANDLER_H
#define _INTERRUPT_HANDLER_H

#include "x86_desc.h"
#include "pcb.h"

#define NR_EXCEPTIONS 			21
#define NR_IRQS 				16

#define NUM_INTEL_DEFINED 		0x20 

#define VEC_LOWEST_EXCEPTION 	0x00
#define VEC_HIGHEST_EXCEPTION 	0x14

#define VEC_LOWEST_IRQ 			0x20
#define VEC_HIGHEST_IRQ 		0x2F

#define VEC_KEYBOARD_INT 		0x21
#define VEC_RTC_INT 			0x28
#define VEC_SYSTEM_CALL 		0x80
#define VEC_PIT_INT				0x20

#define USER_LEVEL				3
#define KERNEL_LEVEL			0

#define IRQ_NAME2(nr) 	nr##_interrupt(void)
#define IRQ_NAME(nr) 	IRQ_NAME2(IRQ##nr)

/* Linkage For 
 Interrupts That Push Error Codes */
#define BUILD_IRQ_ERRCODE(nr) \
void IRQ_NAME(nr); \
__asm__( \
"\n.align 4\n" \
"IRQ" #nr "_interrupt:\n\t" \
"push $(" #nr ") ; " \
"call common_handler;" \
"addl $8, %esp;" \
"iret;");

/* Linkage For
 Interrupts That Do Not Push Error Codes */
#define BUILD_IRQ(nr) \
void IRQ_NAME(nr); \
__asm__( \
"\n.align 4\n" \
"IRQ" #nr "_interrupt:\n\t" \
"pushl $-1;" \
"push $(" #nr ") ; " \
"call common_handler;" \
"addl $8, %esp;" \
"iret;");

/* System Call Linkage */
#define BUILD_SYSCALL(nr) \
void IRQ_NAME(nr); \
__asm__( \
"\n.align 4\n" \
"IRQ" #nr "_interrupt:\n\t" \
"jmp system_call;");

/* Save Regs for Interrupts */ 
#define SAVE_ALL \
asm ( \
"\n.align 4\n" \
"pushl %es;" \
"pushl %ds;" \
"pushl %eax;" \
"pushl %ebp;" \
"pushl %edi;" \
"pushl %esi;" \
"pushl %edx;" \
"pushl %ecx;" \
"pushl %ebx;" \
"movl $0x002B, %edx;" \
"movl %edx, %ds;" \
"movl %edx, %es;");

/* Restore Regs for Interrupts */
#define RESTORE_ALL \
asm ( \
"\n.align 4\n" \
"popl %ebx;" \
"popl %ecx;" \
"popl %edx;" \
"popl %esi;" \
"popl %edi;" \
"popl %ebp;" \
"popl %eax;" \
"popl %ds;" \
"popl %es;");

/* Inits IDT Table */
void init_idt(void);

extern int32_t is_exception;

#endif
