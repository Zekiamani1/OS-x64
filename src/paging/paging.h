#ifndef _PAGING_H
#define _PAGING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Note: MB often referring to MiB in context of memory management
#define SYSTEM_MEMORY_MB     128

// Number of entries in a page directory/page table/pdpt/pml4
#define PAGE_ENTRY_COUNT     512

// Page Frame (PF) Size: 4 KiB
#define PAGE_FRAME_SIZE      0x1000u
#define KERNEL_RESERVED_PAGE_FRAME_COUNT (0x400000u / PAGE_FRAME_SIZE)
#define RECURSIVE_PAGE_TABLES_VIRTUAL_ADDRESS 0xFFC00000u
#define RECURSIVE_PAGE_DIRECTORY_INDEX 1023u

// Maximum usable page frame. Default count: 128 MiB / 4 KiB = 32768 frames
#define PAGE_FRAME_MAX_COUNT ((SYSTEM_MEMORY_MB << 20) / PAGE_FRAME_SIZE)

// Operating system page directory, using 4 KiB pages
extern struct PageDirectory _paging_kernel_page_directory;
extern struct PageTable _paging_kernel_page_table;

/**
 * @brief PML4 entry flags for a 4 KiB page table.
 * 
 * @param present_bit Indicate whether this entry is exist or not
 * @param write_bit Indicate whether this page is writable
 * @param us_bit Indicate whether this page can be accessed from user mode
 * @param pwt_bit Page-level write through
 * @param pcd_bit Page-level cache disable
 * @param accessed_bit Indicate whether this page has been accessed
 * @param ignored_bit Ignored for a 4 KiB PDE
 * @param use_pagesize_4_mb Must be zero: this kernel uses 4 KiB pages.
 */
struct PML4EntryFlag {
    uint8_t present_bit           : 1;
    uint8_t write_bit             : 1;
    uint8_t us_bit                : 1;
    uint8_t pwt_bit               : 1;
    uint8_t pcd_bit               : 1;
    uint8_t accessed_bit          : 1;
    uint8_t ignored_bit           : 1;
    uint8_t ps_bit                : 1;
} __attribute__((packed));
/**
 * @brief PML4 entry for a 4 KiB page table.
 *
 * @param flag            Contain 8-bit page directory entry flag
 * @param global_page     Is this page translation global & cannot be flushed?
 * ...
 * @param reserved_2      Reserved bit (1-bit)
 * @param page_table_base_address Bits 31:12 of the physical page-table base address.
 */
struct PML4Entry {
    struct PML4EntryFlag flag;
    uint64_t ignored                 : 4;
    uint64_t pdpt_base_address       : 36;
    uint64_t reserved                : 4; 
    uint64_t ignored_52_62           : 11; 
    uint64_t execute_disable         : 1; 
} __attribute__((packed));


/**
 * @brief PDPT entry flags for a 4 KiB page table.
 * 
 * @param present_bit Indicate whether this entry is exist or not
 * @param write_bit Indicate whether this page is writable
 * @param us_bit Indicate whether this page can be accessed from user mode
 * @param pwt_bit Page-level write through
 * @param pcd_bit Page-level cache disable
 * @param accessed_bit Indicate whether this page has been accessed
 * @param ignored_bit Ignored for a 4 KiB PDE
 * @param use_pagesize_4_mb Must be zero: this kernel uses 4 KiB pages.
 */
struct PDPTEntryFlag {
    uint8_t present_bit           : 1;
    uint8_t write_bit             : 1;
    uint8_t us_bit                : 1;
    uint8_t pwt_bit               : 1;
    uint8_t pcd_bit               : 1;
    uint8_t accessed_bit          : 1;
    uint8_t ignored_bit           : 1;
    uint8_t ps_bit                : 1;
} __attribute__((packed));
/**
 * @brief PDPT entry for a 4 KiB page table.
 *
 * @param flag            Contain 8-bit page directory entry flag
 * @param global_page     Is this page translation global & cannot be flushed?
 * ...
 * @param reserved_2      Reserved bit (1-bit)
 * @param page_table_base_address Bits 31:12 of the physical page-table base address.
 */
struct PDPTEntry {
    struct PDPTEntryFlag flag;
    uint64_t ignored                 : 4;
    uint64_t pd_base_address         : 36;
    uint64_t reserved                : 4; 
    uint64_t ignored_52_62           : 11; 
    uint64_t execute_disable         : 1; 
} __attribute__((packed));

/**
 * @brief Page-directory entry flags for a 4 KiB page table.
 * 
 * @param present_bit Indicate whether this entry is exist or not
 * @param write_bit Indicate whether this page is writable
 * @param us_bit Indicate whether this page can be accessed from user mode
 * @param pwt_bit Page-level write through
 * @param pcd_bit Page-level cache disable
 * @param accessed_bit Indicate whether this page has been accessed
 * @param ignored_bit Ignored for a 4 KiB PDE
 * @param use_pagesize_4_mb Must be zero: this kernel uses 4 KiB pages.
 */
struct PageDirectoryEntryFlag {
    uint8_t present_bit           : 1;
    uint8_t write_bit             : 1;
    uint8_t us_bit                : 1;
    uint8_t pwt_bit               : 1;
    uint8_t pcd_bit               : 1;
    uint8_t accessed_bit          : 1;
    uint8_t ignored_bit           : 1;
    uint8_t ps_bit                : 1;
} __attribute__((packed));
/**
 * @brief Page-directory entry for a 4 KiB page table.
 *
 * @param flag            Contain 8-bit page directory entry flag
 * @param global_page     Is this page translation global & cannot be flushed?
 * ...
 * @param reserved_2      Reserved bit (1-bit)
 * @param pt_base_address Bits 31:12 of the physical page-table base address.
 */
struct PageDirectoryEntry {
    struct PageDirectoryEntryFlag flag;
    uint64_t ignored                 : 4;
    uint64_t pt_base_address         : 36;
    uint64_t reserved                : 4; 
    uint64_t ignored_52_62           : 11; 
    uint64_t execute_disable         : 1; 
} __attribute__((packed));

/**
 * @brief Page-table entry flags for a mapped 4 KiB page.
 */
struct PageTableEntryFlag {
    uint8_t present_bit           : 1;
    uint8_t write_bit             : 1;
    uint8_t us_bit                : 1;
    uint8_t pwt_bit               : 1;
    uint8_t pcd_bit               : 1;
    uint8_t accessed_bit          : 1;
    uint8_t dirty_bit             : 1;
    uint8_t pat_bit               : 1;
} __attribute__((packed));


/**
 * @brief Page-table entry for a mapped 4 KiB page.
 */
struct PageTableEntry {
    struct PageTableEntryFlag flag;
    uint64_t global_page             : 1;
    uint64_t ignored                 : 3;
    uint64_t frame_address           : 36;
    uint64_t reserved                : 4; 
    uint64_t ignored_52_62           : 11; 
    uint64_t execute_disable         : 1; 

} __attribute__((packed));

/**
 * @brief Page Table/Page Directory/PDPT/PML4, contain array of respective entries.
 * @note This data structure is volatile (can be modified from outside this code, check "C volatile keyword"). 
 * MMU operation, TLB hit & miss also affecting this data structure (dirty, accessed bit, etc).
 * 
 * @warning Address must be aligned in 4 KB (listed on Intel Manual), use __attribute__((aligned(0x1000))), 
 *   unaligned definition of PageDirectory will cause triple fault
 * 
 * @param table Fixed-width array of PageDirectoryEntry with size PAGE_ENTRY_COUNT
 */
struct PML4 {
    struct PML4Entry table[PAGE_ENTRY_COUNT];
} __attribute__((packed));

struct PDPT {
    struct PDPTEntry table[PAGE_ENTRY_COUNT];
} __attribute__((packed));
struct PageDirectory {
    struct PageDirectoryEntry table[PAGE_ENTRY_COUNT];
} __attribute__((packed));
struct PageTable {
    struct PageTableEntry table[PAGE_ENTRY_COUNT];
} __attribute__((packed));
#endif
