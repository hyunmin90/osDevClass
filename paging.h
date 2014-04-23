/* paging.h - Header file for paging.c
 * vim:ts=4 noexpandtab
 */

#ifndef _PAGING_H
#define _PAGING_H

#include "x86_desc.h"

#define PAGE_SIZE_4K 4096
#define PAGE_TABLE_SIZE PAGE_SIZE_4K		 /* Size of a page table in bytes */
#define NUM_PDE 1024				 /* Number of page directory entry in a page directory */
#define NUM_PTE 1024				 /* Number of page table entry in a page table */

#define PAGE_SIZE_4M 0x400000
#define PAGE_BEGINNING_ADDR_4M PAGE_SIZE_4M      /* Physical memory address of where 4M pages begin */

#define PAGING_PRESENT 0x1 			 /* present in physical memory? */
#define PAGING_READ_WRITE 0x2		 /* 0: read-only, 1: read and write */
#define PAGING_USER_SUPERVISOR 0x4   /* 0: supervisor privilege , 1: user privilege */
#define PAGING_WRITE_THROUGH 0x8 	 /* 0: write-back caching, 1: write-through caching */
#define PAGING_CACHE_DISABLED 0x10 	 /* 0: cache is enabled, 1: cache is prevented */
#define PAGING_ACCESSED 0x20 		 /* set when page accessed */
#define PAGING_DIRTY 0x40		     /* Set when written */
#define PAGING_PAGE_SIZE 0x80 		 /* 0: 4KB page size, 1: 4MB page size */
#define PAGING_GLOBAL_PAGE 0x100 	 /* set when global page */

#define PAGE_BASE_ADDRESS_4K(addr) (addr & 0xFFFFF000)
#define PAGE_BASE_ADDRESS_4M(addr) (addr & 0xFFC00000)

#define PAGE_DIR_OFFSET(addr) ((addr & 0xFFC00000) >> 22)
#define PAGE_TABLE_OFFSET(addr) ((addr & 0x003FF000) >> 12)




/* Page Directory Entry */
typedef union pde_t {
	uint32_t val;
	struct {
		uint32_t present : 1;
		uint32_t read_write : 1;
		uint32_t user_supervisor : 1;
		uint32_t write_through : 1;
		uint32_t cache_disabled : 1;
		uint32_t accessed : 1;
		uint32_t dirty : 1;
		uint32_t page_size : 1;
		uint32_t global_page : 1;
		uint32_t available : 3;
		uint32_t table_base_addr : 20;
	} __attribute__((packed));
} pde_t;
 
/* Page Table Entry */
typedef union pte_t {
	uint32_t val;
	struct {
		uint32_t present : 1;
		uint32_t read_write : 1;
		uint32_t user_supervisor : 1;
		uint32_t write_through : 1;
		uint32_t cache_disabled : 1;
		uint32_t accessed : 1;
		uint32_t dirty : 1;
		uint32_t page_table_attr_idx : 1;
		uint32_t global_page : 1;
		uint32_t available : 3;
		uint32_t table_base_addr : 20;
	} __attribute__((packed));
} pte_t;




void init_paging(void);
void enable_global_pages(uint32_t start_addr, uint32_t end_addr);

int32_t map_page(uint32_t virt_addr, uint32_t phys_addr, uint32_t flag, pde_t* pg_dir);

int32_t map_page_vid(uint32_t virt_addr, uint32_t phys_addr, uint32_t flag, pde_t* pg_dir);

pde_t* get_pg_dir(int32_t proc_index);
void set_cr3_reg(pde_t* pg_dir);
int32_t cleanup_pg_dir(pde_t* pg_dir);

extern pde_t pg_dir[];
#endif /* _PAGING_H */


