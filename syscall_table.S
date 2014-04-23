.globl sys_call_table
.extern sys_halt
.extern sys_execute
.extern sys_read
.extern sys_write
.extern sys_open
.extern sys_close
.extern sys_getargs
.extern sys_vidmap
.extern sys_sethandler
.extern sys_sigreturn



sys_call_table:
	.long halt
	.long sys_execute
	.long sys_read
	.long sys_write
	.long sys_open
	.long sys_close
	.long sys_getargs
	.long sys_vidmap
	.long sys_set_handler
	.long sys_sigreturn


