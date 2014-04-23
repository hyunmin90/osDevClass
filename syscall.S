
# syscall.S - Set up x86 segment descriptors, descriptor tables 
# vim:ts=4 noexpandtab

#define ASM     1
#include "x86_desc.h"
#define MAX_NUM_SYS_CALL 10
#define DUMMY -1

.globl RESTORE_INT_REGS

.macro SAVE_ALL
	pushl %fs
	pushl %es
	pushl %ds 
	pushl %eax
	pushl %ebp
	pushl %edi
	pushl %esi 
	pushl %edx
	pushl %ecx
	pushl %ebx
	movl $KERNEL_DS, %edx 
	movl %edx, %ds
	movl %edx, %es
.endm

.macro RESTORE_INT_REGS
	popl %ebx
	popl %ecx
	popl %edx
	popl %esi
	popl %edi
	popl %ebp
	popl %eax
.endm

.macro RESTORE_REGS
	RESTORE_INT_REGS
	popl %ds
	popl %es
	popl %fs
.endm

.globl system_call


######################
# System Call Handler
# DESCRIPTION: 
#
#
######################
system_call:
	pushl $(DUMMY);		# PUSH: Dummy
	pushl %eax			# PUSH: save orig_eax
	SAVE_ALL		

	cmpl $(MAX_NUM_SYS_CALL), %eax 	# check for bad system call
	ja syscall_badsys
	cmpl $0, %eax 
	jbe syscall_badsys

syscall_call:
	subl $1, %eax 
	call *sys_call_table(, %eax, 4) 	

sys_exit:
	movl %eax, 24(%esp)			# Store Return code into User Mode %eax
resume_userspace:
	cli
	RESTORE_REGS
	addl $8, %esp	# POP: Clean up Stack
	iret
syscall_badsys:
	movl $-1, 24(%esp)	# Return
	jmp resume_userspace


