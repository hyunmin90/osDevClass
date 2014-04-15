#include "system_call.h"
#include "lib.h"
#include "syscall_exec.h"

int32_t halt(uint8_t status)
{
	printf("%d\n", status);
	printf("halt\n");
	return 0;
}

int32_t sys_execute(syscall_struct_t syscall_struct)
{
	printf("execute\n");
	return do_execute(syscall_struct);
}

int32_t sys_read(int32_t fd, void* buf, int32_t nbytes)
{
	return 0;
}

int32_t sys_write(int32_t fd, const void* buf, int32_t nbytes)
{
	return 0;
}

int32_t sys_open (const uint8_t* filename)
{
	return 0;
}

int32_t sys_close (int32_t fd)
{
	return 0;
}

int32_t sys_getargs (uint8_t* buf, int32_t nbytes)
{
	return 0;
}

int32_t sys_vidmap(uint8_t** screen_start)
{
	return 0;
}

int32_t sys_set_handler(int32_t signum, void* handler_address)
{
	return 0;
}

int32_t sys_sigreturn(void)
{
	return 0;
}
