#ifndef _INTERRUPT_HANDLER_H
#define _INTERRUPT_HANDLER_H

#include "x86_desc.h"
#define USER_CS 0x0023
#define USER_DS 0x002B

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

#define USER_LEVEL				3
#define KERNEL_LEVEL			0

#define IRQ_NAME2(nr) 	nr##_interrupt(void)
#define IRQ_NAME(nr) 	IRQ_NAME2(IRQ##nr)

/* Create a assembly linkage for an interrupt handler */
#define BUILD_IRQ_ERRCODE(nr) \
void IRQ_NAME(nr); \
__asm__( \
"\n.align 4\n" \
"IRQ" #nr "_interrupt:\n\t" \
"push $(" #nr ") ; " \
"call common_handler;" \
"addl $8, %esp;" \
"iret;");

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

#define BUILD_SYSCALL(nr) \
void IRQ_NAME(nr); \
__asm__( \
"\n.align 4\n" \
"IRQ" #nr "_interrupt:\n\t" \
"jmp system_call;");

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

void init_idt(void);
void build_irq(const int irq_num);


#endif
