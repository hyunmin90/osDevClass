/* paging.c - Initializes paging for utilization of virtual memory
 * vim:ts=4 noexpandtab
 */

#include "paging.h"
#include "lib.h"
#include "pcb.h"

#ifndef LOG
#define LOG(str, ...)                                                   \
    do { if (DEBUG_MODE) printf(str,  ## __VA_ARGS__); } while (0)
#endif

/* Page Directory */
pde_t pg_dir[NUM_PDE] __attribute__((aligned(PAGE_TABLE_SIZE)));
/* Page Table */
pte_t pg_table[NUM_PTE] __attribute__((aligned(PAGE_TABLE_SIZE)));

pde_t pg_dirs[MAX_NUM_PROCESS][NUM_PDE] __attribute((aligned(PAGE_TABLE_SIZE)));

/* init_paging()
   Initialize Paging. 
   Setup 0 ~ 4 MB region with 4KB paging options
   Setup 4MB and higher region with 4MB paging option
   Initialize Kernel's image(4MB ~ 8MB) and video memory(0xB000 ~) with
   identity mapping strategy(location of physical memory and virtual memory are same)
   
   Input : None
   Output : None
   Side Effects : Value of CR3 is updated to physical address of Page Directory
                  First and second entries of page directory are set up
                  A 4KB page corresponding to video memory is set up
                  Paginig is enabled

   Page Directory Entry(4KB) / 
   +---------------------------+-------+---+----+---+---+-----+-----+-----+-----+---+
   | 31                     12 | 11  9 | 8 | 7  | 6 | 5 |  4  |  3  |  2  |  1  | 0 |
   +---------------------------+-------+---+----+---+---+-----+-----+-----+-----+---+
   |  Page-Table Base Address  | Avail | G | PS | 0 | A | PCD | PWT | U/S | R/W | P |
   +---------------------------+-------+---+----+---+---+-----+-----+-----+-----+---+

   Page Table Entry(4KB)
   +---------------------------+-------+---+----+---+---+-----+-----+-----+-----+---+
   | 31                     12 | 11  9 | 8 | 7  | 6 | 5 |  4  |  3  |  2  |  1  | 0 |
   +---------------------------+-------+---+----+---+---+-----+-----+-----+-----+---+
   |  Page Base Address        | Avail | G | PS | D | A | PCD | PWT | U/S | R/W | P |
   +---------------------------+-------+---+----+---+---+-----+-----+-----+-----+---+

   Page Directory Entry(4MB)
   +-------------------+----------+-----+-------+---+----+---+---+-----+-----+-----+-----+---+
   |31               22|21      13| 12  | 11  9 | 8 | 7  | 6 | 5 |  4  |  3  |  2  |  1  | 0 |
   +-------------------+----------+-----+-------+---+----+---+---+-----+-----+-----+-----+---+
   | Page Base Address | Reserved | PAT | Avail | G | PS | D | A | PCD | PWT | U/S | R/W | P |
   +-------------------+----------+-----+-------+---+----+---+---+-----+-----+-----+-----+---+
*/
void init_paging(void) {
    /* Initialize first 4KB Page directory 
       and first page directory entry to be pointing to 4KB page table */
    pg_dir[0].val = PAGE_BASE_ADDRESS_4K(((int)pg_table)) | PAGING_READ_WRITE | PAGING_PRESENT;
	
    /* Initialize the second page directory(at 4MB) where kernel resides */
    /* All pages starting from 4MB are 4MB in size instead of 4KB */
    enable_global_pages(PAGE_BEGINNING_ADDR_4M, PAGE_BEGINNING_ADDR_4M + PAGE_SIZE_4M);
	
    /* Setup pages for video memory. We need one 4KB pages since 2B * 80 * 25 ~ 4000 */
    enable_global_pages(VIDEO, VIDEO + PAGE_SIZE_4K);
	
    /* Set CR3 to be physical address of Page Directory */
    set_cr3_reg(pg_dir);

    /* Set large pages(4MB paging) available; Set PSE(bit 4 of CR4) */
    unsigned int cr4;
    asm volatile("mov %%cr4, %0": "=b"(cr4));
    cr4 |= 0x10;
    asm volatile("mov %0, %%cr4"::"b"(cr4));
	
    /* Turn on paging; Set PG flag(bit31 of CR0) */
    unsigned int cr0;
    asm volatile("mov %%cr0, %0": "=b"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0"::"b"(cr0));
}

/* enable_global_pages()
   Enable paging for the given physical address range
   Input : start_addr -- physical memory address for the start of the region
           end_addr -- physical memory address for the end of the region
   Output : None
   Side Effects: Enable global and identitypaging for given physical address range
 */
void enable_global_pages(uint32_t start_addr, uint32_t end_addr) {
    if (start_addr >= end_addr) {
        return;
    } else if (start_addr < PAGE_BEGINNING_ADDR_4M && end_addr > PAGE_BEGINNING_ADDR_4M) {
        /* Split the range and call recursively */
        enable_global_pages(start_addr, PAGE_BEGINNING_ADDR_4M);
        enable_global_pages(PAGE_BEGINNING_ADDR_4M, end_addr);
    } else if (end_addr < PAGE_BEGINNING_ADDR_4M) {
        /* In case the end_addr is smaller than 4MB, enable 4K page table entries */
        uint32_t start_page = start_addr / PAGE_SIZE_4K;
        /* Assuming end_addr - start_addr != 0 */
        uint32_t num_pages = 1 + (((end_addr - start_addr) - 1) / PAGE_SIZE_4K);
        uint32_t i;
        for (i = start_page; i < start_page + num_pages; i++) {
            pg_table[i].val = PAGE_BASE_ADDRESS_4K(i * PAGE_SIZE_4K) | PAGING_GLOBAL_PAGE |
                PAGING_READ_WRITE | PAGING_PRESENT;   
        }
    } else if (start_addr >= PAGE_BEGINNING_ADDR_4M) {
        /* In case start_addr is greater than or equal to 4MB, enable 4M page directory entries */
        uint32_t start_page = start_addr / PAGE_SIZE_4M;
        /* Assuming end_addr - start_addr != 0 */
        uint32_t num_pages = 1 + (((end_addr - start_addr) - 1) / PAGE_SIZE_4M);
        uint32_t i;
        for (i = start_page; i < start_page + num_pages; i++) {
            pg_dir[i].val = PAGE_BASE_ADDRESS_4M((i * PAGE_SIZE_4M)) | PAGING_GLOBAL_PAGE | PAGING_PAGE_SIZE |
                PAGING_READ_WRITE | PAGING_PRESENT;
        }
    } else {
        printf("enable_global_pages():Illegal state reached\n");
    }
}

int32_t map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flag, pde_t* pg_dir) {
    if (virt_addr < PAGE_BEGINNING_ADDR_4M) {
        // Let's not allow mapping under 4MB for now
        LOG("map_page function doesn't allow mapping under 4MB virtual address\n");
        return -1;
    }
    pde_t* pde = &pg_dir[(PAGE_BASE_ADDRESS_4M(virt_addr) >> 22)];
    if (pde->val & PAGING_PRESENT) {
        /* Since we are not implementing swap now, treat PRESENT bit as existence of pde/pte or not*/
        LOG("You cannot map a page that is already mapped(Present bit is 1 already)\n");
        return -1;
    }
    uint32_t read_write = flag & PAGING_READ_WRITE;
    uint32_t global_page = flag & PAGING_GLOBAL_PAGE;
    uint32_t user_supervisor = flag & PAGING_USER_SUPERVISOR;
    
    pde->val = PAGE_BASE_ADDRESS_4M(phys_addr) | PAGING_PAGE_SIZE | PAGING_PRESENT |
        read_write | global_page | user_supervisor;
    return 0;
}

pde_t* get_pg_dir(int32_t proc_index) {
    if (proc_index < 0 || proc_index >= MAX_NUM_PROCESS) {
        LOG("process index out of bound");
        return NULL;
    }
    return pg_dirs[proc_index];
}

void set_cr3_reg(pde_t* pg_dir) {
    /* Set CR3 to be physical address of Page Directory */
    asm volatile("mov %0, %%cr3;"::"b" (pg_dir));
}
