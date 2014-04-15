#include "x86_desc.h"
#include "lib.h"
#include "file_system.h"
#include "system_call.h"
#include "pcb.h"

#define MAX_CMD_NAME_LENGTH 32
#define MAX_CMD_ARG_LENGTH 32
#define TASK_PAGE_VIRT_ADDR 0x8000000  /* 128MB */
#define TASK_BEGIN_VIRT_ADDR 0x8048000
#define TASK_ENTRY_PTR_VIRT_ADDR 0x8000000
#define LOADER_BUFFER_SIZE 1024

static int32_t parse_command(const int8_t* command, int8_t* exec_name, int8_t* exec_args);
static int32_t check_executable(const int8_t* exec_name, uint32_t* entry_addr);
static int32_t load_executable(const int8_t* exec_name);

// TODO move to lib.c
int32_t index_of_char(const int8_t* str, int8_t c) {
    if (str == NULL) {
        return -1;
    }
    int i = 0;
    while (1) {
        if (str[i] == NULL) {
            return -1;
        } else if (str[i] == c) {
            return i;
        } else {
            i++;
        }
    }
}

int32_t do_execute(syscall_struct_t syscall_struct) {
    LOG("do_execute called\n");
    int8_t* command = (int8_t *) syscall_struct.ebx;

    /* TODO need to find a blank space to setup new PCB, kernel stack 
       and setup ESP0, SS0, and current ESP
     */
    pcb_t* new_pcb_ptr = get_new_pcb_ptr();

    /* Initialize PCB */
    /* TODO Need to do open_terminal somewhere */
    init_pcb(new_pcb_ptr);
    
    /* Parse command */
    if (parse_command(command, new_pcb_ptr->cmd_name, new_pcb_ptr->cmd_args) != 0) {
        LOG("parse failed.\n");
        return -1;
    }
    const int8_t* exec_name = new_pcb_ptr->cmd_name;

    /* Exec check & file existence check */
    uint32_t entry_addr;
    if (check_executable(exec_name, &entry_addr) != 0) {
        LOG("File is not an executable or the file does not exist\n");
        return -1;
    }

    /* Setup Paging. Virt 128MB -> Physical 8MB, 12MB, 16MB, ... */
    /* TBD Also need to map Kernel page at 4MB and Video memory */
    pde_t* pg_dir = get_pg_dir(get_proc_index(new_pcb_ptr));
    if ((map_page(TASK_PAGE_VIRT_ADDR, PHYSICAL_MEM_8MB + (PAGE_SIZE_4M * get_proc_index(new_pcb_ptr)), 
                  PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, pg_dir) != 0) ||
        (map_page(PAGE_BEGINNING_ADDR_4M, PAGE_BEGINNING_ADDR_4M, 
                  PAGING_USER_SUPERVISOR | PAGING_READ_WRITE | PAGING_GLOBAL_PAGE, pg_dir) != 0)) {
        LOG("Failed to map virtual memory for new process\n");
        return -1;
    }
    /* Load the newly mapped Paging scheme */
    set_cr3_reg(pg_dir);
    /* TODO update PCB's pd_dir pointer */

    /* Load the executable file */
    if (load_executable(exec_name) != 0) {
        LOG("Failed to load executable");
        return -1;
    }

    /* Manipulate the TSS's ESP0 and SS0 to point to new process's stack */
    tss.ss0 = KERNEL_DS;
    tss.esp0 = PHYSICAL_MEM_8MB - (KERNEL_STACK_SIZE * get_proc_index(new_pcb_ptr));
    /* TBD Do we need ltr instruction? Maybe not? */

    /* Manipulate the kernel stack to point to return to process's user space */
    syscall_struct.cs = USER_CS;
    syscall_struct.esp = TASK_PAGE_VIRT_ADDR + (PAGE_SIZE_4M * get_proc_index(new_pcb_ptr));
    syscall_struct.ss = USER_DS;
    syscall_struct.return_addr = entry_addr;
    asm volatile("movl %0, %%edx;"
                 "movl %%edx, %%ds;"
                 "movl %%edx, %%es;"::"b" (USER_DS));

    /* Switch into user code */
    asm volatile("iret;");
    /* This is where HALT instruction should come back to */

    return 0;
}

static int32_t parse_command(const int8_t* command, int8_t* exec_name, int8_t* exec_args) {
    if (command == NULL) {
        LOG("command is null\n");
        return -1;
    } else if (command[0] == ' ') {
        LOG("first character of command shouldn't be SPACE character.\n");
        return -1;
    }
    int32_t spaceIndex = index_of_char(command, ' ');
    if (spaceIndex == -1) {
        /* If no extra argument is there, just copy executable name */
        strcpy(exec_name, command);
        return 0;
    } else {
        strncpy(exec_name, command, spaceIndex);

        int32_t argsIndex = -1;
        while (command[spaceIndex] != NULL) {
            if (command[spaceIndex] == ' ') {
                spaceIndex++;
            } else {
                argsIndex = spaceIndex;
                break;
            }
        }
        /* If there was an argument in command */
        if (argsIndex != -1) {
            strcpy(exec_args, &command[argsIndex]);
        }
        return 0;
    }
}

static int32_t check_executable(const int8_t* exec_name, uint32_t* entry_addr) {
    file_header_t file_header;
    /* Open the file and read the header */
    inode_t* inode_ptr = (inode_t *) open_file((uint8_t *) exec_name);
    if (!inode_ptr) {
        LOG("Nonexistent file\n");
        return -1;
    }

    /* If number of read bytes is less than size of file header,
       first four bytes are not 0x7f, 'E', 'L', 'F',
       or entry pointer to executable is not in the correct range, return error */
    int32_t count = read_file(inode_ptr, 0, file_header.data, FILE_HEADER_SIZE);
    if (count < FILE_HEADER_SIZE) {
        LOG("The bytes read are not enough to analyze the executable file\n");
        return -1;
    } else if (!(file_header.elf[0] == 0x7f && file_header.elf[1] == 'E' &&
                 file_header.elf[2] == 'L' && file_header.elf[3] == 'F')) {
        LOG("The magic number 7f E L F for executable is not present.\n");
        return -1;
    } else if (((uint32_t)file_header.entry_ptr < TASK_PAGE_VIRT_ADDR || 
                (uint32_t)file_header.entry_ptr > TASK_PAGE_VIRT_ADDR + PAGE_SIZE_4M)) {
        LOG("Entry pointer for executable is not in correct range\n");
        return -1;
    }
    *entry_addr = (uint32_t) file_header.entry_ptr;
    return 0;
}

static int32_t load_executable(const int8_t* exec_name) {
    uint8_t buf[LOADER_BUFFER_SIZE];
    inode_t* inode_ptr = (inode_t *) open_file((uint8_t *) exec_name);
    if (!inode_ptr) {
        LOG("Nonexistent file\n");
        return -1;
    }
    
    /* Copy executable */
    int32_t bytes_read;
    int32_t count = 0;
    while ((bytes_read = read_file(inode_ptr, 0, buf, LOADER_BUFFER_SIZE)) != 0) {
        memcpy((uint8_t *)(TASK_BEGIN_VIRT_ADDR + count), &buf, bytes_read);
        count += bytes_read;
    }

    return 0;
}
