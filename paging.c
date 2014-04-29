/* paging.c - Initializes paging for utilization of virtual memory
 * vim:ts=4 noexpandtab
 */

#include "paging.h"
#include "lib.h"
#include "pcb.h"
#include "debug.h"

/* Page Directory */
pde_t pg_dir[NUM_PDE] __attribute__((aligned(PAGE_TABLE_SIZE)));
/* Page Table */
pte_t pg_table[NUM_PTE] __attribute__((aligned(PAGE_TABLE_SIZE)));

pde_t pg_dirs[MAX_NUM_PROCESS][NUM_PDE] __attribute__((aligned(PAGE_TABLE_SIZE)));

pte_t pg_tables[MAX_NUM_PROCESS][NUM_PTE] __attribute__((aligned(PAGE_TABLE_SIZE)));
/* Page Table for video mapping */
pte_t pg_tables_vid[MAX_NUM_PROCESS][NUM_PTE] __attribute__((aligned(PAGE_TABLE_SIZE)));

extern char* video_mem;

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
    //map_page(VIDEO, VIDEO, 
               //PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, pg_dir);
    enable_global_pages(VIDEO, VIDEO + PAGE_SIZE_4K);
    enable_global_pages(VIDEO_BUF_1, VIDEO_BUF_1 + PAGE_SIZE_4K);
    enable_global_pages(VIDEO_BUF_2, VIDEO_BUF_2 + PAGE_SIZE_4K);
    enable_global_pages(VIDEO_BUF_3, VIDEO_BUF_3 + PAGE_SIZE_4K);
    map_page(USER_VIDEO, VIDEO,
        PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, pg_dir);
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

    video_mem = (char* )USER_VIDEO;
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
        printf("enable_global_pages():Illegal state reached");
    }
}

/*get_proc_index_for_pg_dir()
  Given pointer to page directory, returns process's index that is bound to given PD
  Input : pg_dir - page directory to search
  Output : index of process in global_pcb_ptrs array that is bound to given page directory
           -1 if no matching process index is found
 */
int32_t get_proc_index_for_pg_dir(pde_t* pg_dir) {
  int i;
  for(i = 0; i < MAX_NUM_PROCESS; i++){
    if(pg_dir == pg_dirs[i])
    break;
  }
  if(i == MAX_NUM_PROCESS)
    return -1;
  else
    return i;
}

/*map_page()
  Map given virtual address to physical address with flag variable, at given pg_dir
  Input : virt_addr - Virtual address of the page to map from
          phys_addr - Physical address of the page to be mapped to
          flag - Flag values to be used; 
                 supports PAGING_READ_WRITE, PAGING_GLOBAL_PAGE, PAGING_USER_SUPERVISOR
          pg_dir - Pointer to page directory to operate on
  Output : 0 on success, -1 on failure
  Side Effects : Update Page Directory Entry and/or Page Directory Entry to newly map 
                 virtual address to physical address
*/
int32_t map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flag, pde_t* cur_pg_dir) {
    if (virt_addr < PAGE_BEGINNING_ADDR_4M) {
      uint32_t read_write = flag & PAGING_READ_WRITE;
      uint32_t global_page = flag & PAGING_GLOBAL_PAGE;
      uint32_t user_supervisor = flag & PAGING_USER_SUPERVISOR;

      /* Find Process # */
      int i = get_proc_index_for_pg_dir(cur_pg_dir);
      if(i == -1 && cur_pg_dir != pg_dir)
        return -1;

      pte_t* cur_pg_table;
      if(cur_pg_dir == pg_dir)
        cur_pg_table = pg_table;
      else
        cur_pg_table = pg_tables[i];
      
      /* Fill in PDE */
      pde_t* pde = &cur_pg_dir[PAGE_DIR_OFFSET(virt_addr)]; 
      if (!(pde->val & PAGING_PRESENT)) {
        pde->val = PAGE_BASE_ADDRESS_4K((uint32_t) cur_pg_table) | PAGING_PRESENT | read_write |
                                        global_page | user_supervisor;
      }

      pte_t* pte = &cur_pg_table[PAGE_TABLE_OFFSET(virt_addr)];
    
      /* Fill in PTE */
      if(pte->val & PAGING_PRESENT)
        return -1;
      pte->val = PAGE_BASE_ADDRESS_4K(phys_addr) | PAGING_PRESENT | read_write |
        read_write | global_page | user_supervisor;
      return 0;
    } else {
      pde_t* pde = &cur_pg_dir[PAGE_DIR_OFFSET(virt_addr)];
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
}

int32_t remap_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flag, pde_t* pg_dir) {
    if (virt_addr < PAGE_BEGINNING_ADDR_4M) {
      uint32_t read_write = flag & PAGING_READ_WRITE;
      uint32_t global_page = flag & PAGING_GLOBAL_PAGE;
      uint32_t user_supervisor = flag & PAGING_USER_SUPERVISOR;
      /* Find Process # */
      int i = get_proc_index_for_pg_dir(pg_dir);
      if(i == -1)
        return -1;

      /* Fill in PDE */
      pde_t* pde = &pg_dir[PAGE_DIR_OFFSET(virt_addr)]; 
      if ((pde->val & PAGING_PRESENT)) {
        pde->val = PAGE_BASE_ADDRESS_4K((uint32_t) pg_tables[i]) | PAGING_PRESENT | read_write |
                                        global_page | user_supervisor;
      }
      else
        LOG("Trying to remap a page that has not been ampped!\n");

      /* Fill in PTE */
      pte_t* pte = &pg_tables[i][PAGE_TABLE_OFFSET(virt_addr)];
      if(!(pte->val & PAGING_PRESENT))
        return -1;
      pte->val = PAGE_BASE_ADDRESS_4K(phys_addr) | PAGING_PRESENT | read_write |
        read_write | global_page | user_supervisor;
      return 0;
    } else {
      pde_t* pde = &pg_dir[PAGE_DIR_OFFSET(virt_addr)];
      if (!(pde->val & PAGING_PRESENT)) {
          /* Since we are not implementing swap now, treat PRESENT bit as existence of pde/pte or not*/
          LOG("You cannot remap a page that is not already mapped(Present bit is 0)\n");
          return -1;
      }
      uint32_t read_write = flag & PAGING_READ_WRITE;
      uint32_t global_page = flag & PAGING_GLOBAL_PAGE;
      uint32_t user_supervisor = flag & PAGING_USER_SUPERVISOR;
      
      pde->val = PAGE_BASE_ADDRESS_4M(phys_addr) | PAGING_PAGE_SIZE | PAGING_PRESENT |
          read_write | global_page | user_supervisor;
      return 0;
    }
}

/*map_page_vid()
  video map paging function - only deals with 4kb paging
  Input : virt_addr - Virtual address of the page to map from
          phys_addr - Physical address of the page to be mapped to
          flag - Flag values to be used; 
                 supports PAGING_READ_WRITE, PAGING_GLOBAL_PAGE, PAGING_USER_SUPERVISOR
          pg_dir - Pointer to page directory to operate on
  Output : 0 on success, -1 on failure
  Side Effects : Update Page Directory Entry and/or Page Directory Entry to newly map 
                 virtual address to physical address
*/
int32_t map_page_vid(uint32_t virt_addr, uint32_t phys_addr, uint32_t flag, pde_t* pg_dir) {
    
      uint32_t read_write = flag & PAGING_READ_WRITE;
      uint32_t global_page = flag & PAGING_GLOBAL_PAGE;
      uint32_t user_supervisor = flag & PAGING_USER_SUPERVISOR;
      /* Find Process # */
      int i = get_proc_index_for_pg_dir(pg_dir);
      if(i == -1)
        return -1;

      /* Fill in PDE */
      pde_t* pde = &pg_dir[PAGE_DIR_OFFSET(virt_addr)]; 
      if (!(pde->val & PAGING_PRESENT)) {
        pde->val = PAGE_BASE_ADDRESS_4K((uint32_t) pg_tables_vid[i]) | PAGING_PRESENT | read_write |
                                        global_page | user_supervisor;
      }

      /* Fill in PTE for video */
      pte_t* pte = &pg_tables_vid[i][PAGE_TABLE_OFFSET(virt_addr)];
      if(pte->val & PAGING_PRESENT)
        return -1;
      pte->val = PAGE_BASE_ADDRESS_4K(phys_addr) | PAGING_PRESENT | read_write |
        read_write | global_page | user_supervisor;
      return 0;
}


/*get_pg_dir()
  Given process's index in global_pcb_ptrs, find the page directory that belongs to 
  given index's PCB
  Input : proc_index - index of PCB in global_pcb_ptrs
  Output : Pointer to page directory
           NULL if not found
 */
pde_t* get_pg_dir(int32_t proc_index) {
    if (proc_index < 0 || proc_index >= MAX_NUM_PROCESS) {
        LOG("process index out of bound");
        return NULL;
    }
    return pg_dirs[proc_index];
}

/*set_cr3_reg()
  Update CR3 register to point to new Page Directory.
  Side Effects : Flush Translate Lookaside Buffer
 */
void set_cr3_reg(pde_t* pg_dir) {
    /* Set CR3 to be physical address of Page Directory */
    asm volatile("mov %0, %%cr3;"::"b" (pg_dir));
}

/*cleanup_pg_dir()
  When page directory is not being used anymore, clean up all page directory entries
  and page table entries corresponding to given pointer by setting them to 0s.
  Input : pg_dir - Pointer to page directory to be cleaned up
  Output : 0 on success, -1 on failure
  Side Effects : Update a Page directory and Page table
 */
int32_t cleanup_pg_dir(pde_t* pg_dir) {
  int i;
  int index;
  for (i = 0; i < NUM_PDE; i++) {
    pg_dir[i].val = NULL;
  }

  index = get_proc_index_for_pg_dir(pg_dir);
  if(index == -1)
    return -1;
  pte_t* cur_pg_table = pg_tables[index];
  for (i = 0; i < NUM_PTE; i++) {
    cur_pg_table[i].val = NULL;
  }
  return 0;
}
