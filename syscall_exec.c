#include "x86_desc.h"
#include "lib.h"
#include "file_system.h"
#include "system_call.h"
#include "pcb.h"
#include "paging.h"
#include "debug.h"

#define MAX_CMD_NAME_LENGTH 32
#define MAX_CMD_ARG_LENGTH 32
#define TASK_PAGE_VIRT_ADDR 0x8000000  /* 128MB */
#define TASK_BEGIN_VIRT_ADDR 0x8048000
#define TASK_ENTRY_PTR_VIRT_ADDR 0x8000000
#define LOADER_BUFFER_SIZE 16
#define TASK_MEM_PADDING 16
#define EFLAGS_BASE 2
#define EFLAGS_STI (1 << 9)

static int32_t parse_command(const int8_t* command, int8_t* exec_name, int8_t* exec_args);
static int32_t check_executable(const int8_t* exec_name, uint32_t* entry_addr);
static int32_t load_executable(const int8_t* exec_name);

extern pcb_t* top_process[NUM_TERMINALS];
extern int32_t num_progs[NUM_TERMINALS];

/*do_execute()
  Execute a new process and set the current process to be parent process of the newly spawned process
  1. Set up a new PCB block
  2. Parse command, extract argument
  3. Check if file exists, and if it is an executable file
  4. Set up a new page directory, and switch CR3 to point to new PD
  5. Load the executable file into 128MB virtual memory
  6. Sets up the stack in order to context switch into user's program
  7. Enter into the user's program with IRET
  8. Upon JMP from HALT system call, return HALT's status

  Input : syscall_struct - the view of the pushed stack of registers
  Output : 0 on success(upon executed process halts)
           256 if process is halted by exception
           0~255 if user program calls halt system call
  Side Effects : Update TSS's ESP0 and SS0 in order to enable the new process
                 can go back and forth in its own kernel stack and user stack.
                 IRET into the user's program
                 User program's HALT system call JMP's into this function
 */
int32_t do_execute(const int8_t* command) {
    asm volatile("cli");
    LOG("do_execute called\n");
    int32_t ret_val = 0;

    pcb_t* new_pcb_ptr = get_new_pcb_ptr();
    if (new_pcb_ptr == NULL) {
        LOG("No more Process for you!\n");
        return -1;
    }

    pcb_t* cur_pcb_ptr = get_pcb_ptr();

    /* Initialize PCB */
    init_pcb(new_pcb_ptr);

    /* Update Parent Process */
    LOG("new process with parent process %d\n", get_proc_index(get_pcb_ptr()));
    new_pcb_ptr->parent_pcb = cur_pcb_ptr;

    /* Newly created process will run in currently displayed terminal */
    new_pcb_ptr -> terminal_num = get_displayed_terminal();

    /* If there is another program already running in same terminal, save ESP,EBP,SS
       in order to be able to come back to parent when Halt system call is called */
    if(num_progs[new_pcb_ptr -> terminal_num] != 0){
        asm volatile("movl %%esp, %0": "=b"(cur_pcb_ptr->esp));
        asm volatile("movl %%ss, %0": "=b"(cur_pcb_ptr->ss0));
        asm volatile("movl %%ebp, %0": "=b"(cur_pcb_ptr->ebp));
        cur_pcb_ptr -> esp0 = tss.esp0;
    }
    
    /* Parse command */
    if (parse_command(command, new_pcb_ptr->cmd_name, new_pcb_ptr->cmd_args) != 0) {
        LOG("parse failed.\n");
        destroy_pcb_ptr(new_pcb_ptr);
        return -1;
    }
    const int8_t* exec_name = new_pcb_ptr->cmd_name;

    /* Exec check & file existence check */
    uint32_t entry_addr;
    if (check_executable(exec_name, &entry_addr) != 0) {
        LOG("File is not an executable or the file does not exist\n");
        destroy_pcb_ptr(new_pcb_ptr);
        return -1;
    }

    /* Setup Paging. Virt 128MB -> Physical 8MB, 12MB, 16MB, ... */
    pde_t* new_pg_dir = get_pg_dir(get_proc_index(new_pcb_ptr));
    

    /* Map the Video memory and Video memory buffers */
    if((map_page(VIDEO_BUF_1, VIDEO_BUF_1,
        PAGING_USER_SUPERVISOR | PAGING_READ_WRITE | PAGING_GLOBAL_PAGE, new_pg_dir) != 0) ||
       (map_page(VIDEO_BUF_2, VIDEO_BUF_2,
        PAGING_USER_SUPERVISOR | PAGING_READ_WRITE | PAGING_GLOBAL_PAGE, new_pg_dir) != 0) ||
       (map_page(VIDEO_BUF_3, VIDEO_BUF_3,
        PAGING_USER_SUPERVISOR | PAGING_READ_WRITE | PAGING_GLOBAL_PAGE, new_pg_dir) != 0) ||
       (map_page(USER_VIDEO, VIDEO,
        PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, new_pg_dir) != 0)) {
        LOG("Failed to map virtual video buffers for new process\n");
        destroy_pcb_ptr(new_pcb_ptr);
        cleanup_pg_dir(new_pg_dir);
        return -1;
    }

    if ((map_page(TASK_PAGE_VIRT_ADDR, PHYSICAL_MEM_8MB + (PAGE_SIZE_4M * get_proc_index(new_pcb_ptr)), 
                  PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, new_pg_dir) != 0) ||
        (map_page(PAGE_BEGINNING_ADDR_4M, PAGE_BEGINNING_ADDR_4M, 
                  PAGING_USER_SUPERVISOR | PAGING_READ_WRITE | PAGING_GLOBAL_PAGE, new_pg_dir) != 0) ||
        (map_page(VIDEO, VIDEO, 
                  PAGING_USER_SUPERVISOR | PAGING_READ_WRITE | PAGING_GLOBAL_PAGE, new_pg_dir) != 0)) {
        LOG("Failed to map virtual memory for new process\n");
        destroy_pcb_ptr(new_pcb_ptr);
        cleanup_pg_dir(new_pg_dir);
        return -1;
    }

    /* Load the newly mapped Paging scheme */
    set_cr3_reg(new_pg_dir);
    if (get_global_pcb() == cur_pcb_ptr) {
        cur_pcb_ptr->pg_dir = pg_dir;
    }
    new_pcb_ptr->pg_dir = new_pg_dir;
    
    /* Load the executable file */
    if (load_executable(exec_name) != 0) {
        LOG("Failed to load executable");
        destroy_pcb_ptr(new_pcb_ptr);
        cleanup_pg_dir(new_pg_dir);
        if(cur_pcb_ptr == get_global_pcb())
            set_cr3_reg(pg_dir);
        else
            set_cr3_reg(cur_pcb_ptr -> pg_dir);
        return -1;
    }

    /* Manipulate the TSS's ESP0 and SS0 to point to new process's stack */
    tss.ss0 = KERNEL_DS;
    tss.esp0 = PHYSICAL_MEM_8MB - (KERNEL_STACK_SIZE * (get_proc_index(new_pcb_ptr) + 1));
    
    /* Push to the kernel stack that will be popped of by the IRET instruction to jump to user program */
    asm volatile("pushl %0"::"b" (USER_DS)); /* Push User Program's SS */
    asm volatile("pushl %0"::"b" (TASK_PAGE_VIRT_ADDR + PAGE_SIZE_4M - TASK_MEM_PADDING)); /* Push User Program's ESP */
    asm volatile("pushl %0"::"b" (EFLAGS_STI | EFLAGS_BASE)); /* Push User Program's EFLAGS with Interrupt enabled */
    asm volatile("pushl %0"::"b" (USER_CS)); /* Push User Program's CS */
    asm volatile("pushl %0"::"b" (entry_addr)); /* Push User Program's Entry Address */

    /* Update the top_process with the new process 
     * Increment the num of programs running */
    int32_t current_terminal = get_displayed_terminal();
    top_process[current_terminal] = new_pcb_ptr;
    num_progs[current_terminal]++;

    /* Update DS, ES to the user space's values */
    asm volatile("movl %0, %%edx;"
                 "movl %%edx, %%ds;"
                 "movl %%edx, %%es;"::"b" (USER_DS));

    /* Switch into user code */
    asm volatile("iret;"
                 ".globl halt_ret;"
                 "halt_ret:");
    /* This is where HALT instruction should come back to */
    /* Assign return value from HALT system call's status */
    asm volatile("movl %%eax, %0":"=b" (ret_val));
    return ret_val;
}

/*parse_command()
  Given a string of command for EXECUTE system call, extract the command
  (From beginnig character until the first SPACE character occurs), and
  extract arguments (all characters after command)
  Input : command - string to parse
          exec_name - pointer to string to fill in parsed command
          exec_args - pointer to string to fill in parsed arguments
  Output : 0 if success
           -1 if failed(if command is NULL or empty)
 */
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

/*check_executable()
  Given name of the executable, find a matching file from the file system,
  check(if found) that the file is an executable file, and fill entry_addr
  with the entry address of the program.
  Input : exec_name - Name of executable file to be found
          entry_addr - pointer to user program's entry point to be filled
  Output : 0 on success, 
           -1 on failure(if file is not found, or file is not an executable)
 */
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

/*load_executable()
  Given the name of file, load the program at virtual memory 0x8048000
  Input : exec_name - Name of the executable file to be loaded
  Output : 0 on success, -1 on failure(file does not exist)
  Side Effects : Update virtual memory page between 128MB ~ 132MB
 */
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
    while ((bytes_read = read_file(inode_ptr, count, buf, LOADER_BUFFER_SIZE)) != 0) {
        memcpy((uint8_t *)(TASK_BEGIN_VIRT_ADDR + count), buf, bytes_read);
        count += bytes_read;
    }

    return 0;
}
