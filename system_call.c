#include "system_call.h"
#include "lib.h"
#include "syscall_exec.h"
#include "file_system.h"
#include "rtc.h"
#include "pcb.h"
#include "x86_desc.h"
#include "debug.h"
#include "keyboard.h" // only for NUM_TERMINALS
#include "interrupt_handler.h"

#define FD_ENTRY_MIN 2
#define FD_ENTRY_MAX 7
#define HALT_ARG_BITMASK 0xFF
#define TASK_BEGIN_VIRT_ADDR 0x8048000
#define HALT_DUE_TO_EXCEPTION 256

extern void* halt_ret;
extern file_ops_t file_ops_ptrs[FILE_OPS_PTRS_SIZE];
extern inode_t* inodes;

pcb_t* top_process[NUM_TERMINALS] = {NULL, NULL, NULL}; 	// Top Running Process's PCB in terminal	
int32_t num_progs[NUM_TERMINALS] = {0, 0 ,0};				// # of Progs running in each Terminal

/* halt()
   Halt System Call
   Terminates Caller Process
   Returns to Parent Process 
   Input : status -- status given by the user program that should be eventually passed to user
   				     who first called system call Execute.
   Output : Returns 1 if keycode is letter
   			0 otherwise
   Side Effect : Checks for a letter
*/
int32_t halt(uint8_t status)
{
	uint32_t status_32bit;
	/* Expand 8-bit argument into 32-bit. If exception caused halt, use 256 as status */
	if (is_exception) {
		status_32bit = HALT_DUE_TO_EXCEPTION;
		// It's safe to do so, because whenever execption handler is reached, it's called inside CLI.
		is_exception = 0;
	} else {
		status_32bit = status;
	}
	LOG("halt with status %d\n", status_32bit);

	pcb_t* current_pcb_ptr = get_pcb_ptr();
	pcb_t* parent_pcb_ptr = current_pcb_ptr->parent_pcb;
	int32_t current_terminal = get_current_terminal();

	/* Close opened files except for stdin, stdout */
	int i;
	for (i = FD_ENTRY_MIN; i <= FD_ENTRY_MAX; i++) {
		if (current_pcb_ptr -> file_array[i].flags) {
			sys_close(i);
		}
	}

	if(num_progs[current_terminal] == 1){
		tss.esp0 = PHYSICAL_MEM_8MB - (KERNEL_STACK_SIZE * (get_proc_index(current_pcb_ptr) + 1));
		tss.ss0 = KERNEL_DS;
		printf("Exiting Last Shell. Firing New Shell\n");
		set_cr3_reg(pg_dir);

		if (cleanup_pg_dir(current_pcb_ptr->pg_dir) != 0) {
			LOG("Fatal error while tearing down page directory.\n");
		}
		if(destroy_pcb_ptr(current_pcb_ptr) != 0) {
			LOG("Cannot Destroy PCB_ptr no matching PCB found.\n");
		}
		num_progs[current_terminal]--;

		uint8_t exec_cmd[15] = "shell";
		if(-1 == sys_execute(exec_cmd)){
			LOG("FATAL ERROR! Shell failed to execute inside Halt!\n");
		}
	}
	if(parent_pcb_ptr == NULL) {
		LOG("No parent PCB pointer presents.\n");
		return -1;
	}
	
	if (current_terminal == get_displayed_terminal()) {
		remap_page(USER_VIDEO, VIDEO,
			PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, parent_pcb_ptr -> pg_dir);
	} else {
		remap_page(USER_VIDEO, get_video_buf_for_terminal(current_terminal),
			PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, parent_pcb_ptr -> pg_dir);
	}

	// TSS:esp0 <- parent's esp0
	tss.esp0 = parent_pcb_ptr->esp0;
	// TSS:ss0 <- parent's ss0
	tss.ss0 = parent_pcb_ptr->ss0;
	
	// virtual memory cleanup: update CR3
	set_cr3_reg(parent_pcb_ptr->pg_dir);
	/* Clean up page directory */
	if (cleanup_pg_dir(current_pcb_ptr->pg_dir) != 0) {
		LOG("Fatal error while tearing down page directory.\n");
	}

	/* Since we are Halting, update the top process 
	as the parent process. Decrement the num_progs */
    top_process[current_terminal] = parent_pcb_ptr;
    num_progs[current_terminal]--;

	if(destroy_pcb_ptr(current_pcb_ptr) != 0) {
		LOG("Cannot Destroy PCB_ptr no matching PCB found.\n");
	}

	/* update esp to point to top of parent's kernel stack
	   update ebp
	   Return Value */
	asm volatile("movl %0, %%eax;"
				 "movl %1, %%esp;"
				 "movl %2, %%ebp;"::
				 "a" (status_32bit), 
				 "b" (parent_pcb_ptr->esp),
				 "c" (parent_pcb_ptr->ebp));

	/* Jump to sys_exec */
	asm volatile("jmp halt_ret");

	/* Never reach here but just return 0 to avoid warning */
	return 0;
}

int32_t sys_execute(const uint8_t* command)
{
	LOG("sys_execute\n");
	return do_execute((int8_t*) command);
}

/* sys_read :
  reads data from the keyboard, a file, device (RTC), or directory
  Input : fd -- the integer index for file descriptor array
  		  buf -- array to be filled in with read data
		  nbytes -- the number of bytes to be read
  Output : the number of bytes read,
  		    or 0 to indicate that the end of the file has been reached
  		    (for normal files and the directory).
           -1 if failed(if command is out of reach)
  Side Effect : None
*/
int32_t sys_read(int32_t fd, void* buf, int32_t nbytes)
{
	LOG("sys_read\n");
	// get current pcb
	pcb_t* pcb = get_pcb_ptr();
	/* Buffer Null Check */
	if(buf == NULL)
		return -1;
	/* Check for out of range fd, or reads on unopened file descriptors or stdout */
	if(fd > FD_ENTRY_MAX || fd < 0 || fd == 1 || ((pcb ->file_array)[fd]).flags == 0)
		return -1;
	// call read function and return number of bytes read
	return (((pcb -> file_array)[fd]).file_ops -> read)(fd, buf, nbytes);
}

/* sys_write
   writes data to keyboard,file, device (RTC), or directory
   Input : fd -- the index in the file descriptor array
   		   buf -- array with data to be written
   		   nbytes -- the number of bytes to be written
   Output : Returns the number of bytes written
   			-1 on failure
   Side Effect : None
*/
int32_t sys_write(int32_t fd, const void* buf, int32_t nbytes)
{
	LOG("sys_write\n");
	pcb_t* pcb = get_pcb_ptr();

	/* Buffer Null Check */
	if(buf == NULL)
		return -1;
	/* Checks for out of range fd, write on unopened file descriptor or stdin */ 
	if(fd > FD_ENTRY_MAX || fd <= 0 || ((pcb -> file_array)[fd]).flags == 0)
		return -1;

	return (((pcb -> file_array)[fd]).file_ops -> write)(fd, 0, buf, nbytes);
}

/* sys_open
   Opens the given file
   Input : filename -- the name of file to be opened
   Output : Return the index of the newly allocated file descriptor
   			-1 on failure
   Side Effect : Allocates an unused file descriptor 
*/
int32_t sys_open (const uint8_t* filename)
{
	LOG("sys_open\n");
	pcb_t* pcb_ptr = get_pcb_ptr();

	/* Find Free Space */
	uint32_t fd_index = find_free_fd_index(pcb_ptr);
	if(fd_index == -1){
		LOG("No Free Space in File Array. Can't Open!");
		return -1;
	}
	/* Check for existing filename */
	dentry_t curr_file;
	if(read_dentry_by_name(filename, &curr_file) == -1)
		return -1;

	/* Update pcb according to filetype */
	(pcb_ptr->file_array)[fd_index].flags = 1;
	(pcb_ptr->file_array)[fd_index].file_position = 0;

	if(curr_file.file_type == FILE_TYPE_RTC) {
		(pcb_ptr->file_array)[fd_index].file_ops = &file_ops_ptrs[RTC_FILE_OPS_IDX];
	} else if (curr_file.file_type == FILE_TYPE_DIR) {
		(pcb_ptr->file_array)[fd_index].file_ops = &file_ops_ptrs[DIR_FILE_OPS_IDX];
	} else {
		(pcb_ptr->file_array)[fd_index].file_ops = &file_ops_ptrs[REG_FILE_OPS_IDX];
	}

	/* Call Open Function */
	if((((pcb_ptr -> file_array)[fd_index]).file_ops -> open)((int32_t) filename) == -1)
		destroy_fd(pcb_ptr, fd_index);

	return fd_index;
}

/* sys_close
   Closes the given file associated with the file descriptor
   Input : fd -- the index of the file to be closed
   Output : 0 on successful close
   		    -1 on failure
   Side Effect : Frees up the corresponding file descriptor
*/
int32_t sys_close (int32_t fd)
{
	LOG("sys_close\n");
	
	/* Check if out of bounds */
	if(fd < FD_ENTRY_MIN || fd > FD_ENTRY_MAX)
		return -1;

	/* Check for closing unopened file descriptor */
	pcb_t* pcb_ptr = get_pcb_ptr();
	if(((pcb_ptr -> file_array)[fd].flags) == 0)
		return -1;

	/* Call Close */
	(((pcb_ptr -> file_array)[fd]).file_ops -> close)((int32_t)(pcb_ptr -> file_array[fd]).inode_ptr);

	/* Empty out file array */
	destroy_fd(pcb_ptr, fd);
	return 0;
}

/* Get Args
   Get_Args System Call
   Read Program's command line arguments 
   Reads it in to user-level-buffer 
   Input : User-Level-Buffer, Number of bytes to be read
   Output : Returns 0 on sucess
   			-1 on failure
   Side Effect : NONE
*/
int32_t sys_getargs (uint8_t* buf, int32_t nbytes)
{
	int i=0;
	int bufferlength=0;
	if(buf==NULL)
		return -1;
	pcb_t* current_pcb_ptr = get_pcb_ptr(); //get the current PCB
	
	for(i=0;i<nbytes;i++) //Checks the buffer length of current pointer's argument
	{
		if((current_pcb_ptr->cmd_args)[i]!=NULL)
		{	bufferlength++; } //Increasing the buffer length
		else
			break;
	}	
	for(i = 0; i < bufferlength + 1; i++) //Copy the command argument to current pointer
		buf[i] = (current_pcb_ptr -> cmd_args)[i];
	
	return 0; //return 0 on scucess
}

/*sys_vidmap :
  maps the text-mode video memory into user space at a pre-set virtual address 
  Input : screen_start - double pointer, which is supposed to point the beginning of
  						  the text-mode video memory
  Output : 0 if success
           -1 if failed(if command is NULL or invalid)
  Side Effect : screen_start will be updated to point the text-mode video memory
*/
int32_t sys_vidmap(uint8_t** screen_start)
{
	// null case
	if(screen_start == NULL)
		return -1;

	// see if it fits into the virtual memory address (128MB - 132MB)
	if((uint32_t) screen_start < (TASK_BEGIN_VIRT_ADDR) ||
		(uint32_t)screen_start > TASK_BEGIN_VIRT_ADDR + PAGE_SIZE_4M)
		return -1;

	
    // 4kb paging directly instead. To Our virtual USER VIDEO
    *screen_start = (uint8_t*)(USER_VIDEO);

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
