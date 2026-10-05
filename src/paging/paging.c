#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "paging.h"
#include "../stdlib/string.h"
#define KERNEL_VIRTUAL_BASE 0xFFFF800000000000ULL
__attribute__((aligned(0x1000))) struct PML4 _paging_kernel_pml4={0};
__attribute__((aligned(0x1000))) struct PDPT _paging_kernel_pdpt={0};
__attribute__((aligned(0x1000))) struct PageDirectory _paging_kernel_page_directory={0};
__attribute__((aligned(0x1000))) struct PageTable _paging_kernel_page_tables[2]={0};
void init_higher_half_kernel(){
    _paging_kernel_pml4.table[0].pdpt_base_address  =  (((uint64_t) &_paging_kernel_pdpt) >> 12) & 0xFFFFFFFFFFULL;
    _paging_kernel_pml4.table[0].flag.present_bit   = 1;
    _paging_kernel_pml4.table[0].flag.write_bit     = 1;

    _paging_kernel_pml4.table[256].pdpt_base_address      =  (((uint64_t) &_paging_kernel_pdpt) >> 12) & 0xFFFFFFFFFFULL;
    _paging_kernel_pml4.table[256].flag.present_bit       = 1;
    _paging_kernel_pml4.table[256].flag.write_bit         = 1;

    _paging_kernel_pdpt.table[0].pd_base_address        =  (((uint64_t) &_paging_kernel_page_directory) >> 12) & 0xFFFFFFFFFFULL;
    _paging_kernel_pdpt.table[0].flag.present_bit       = 1;
    _paging_kernel_pdpt.table[0].flag.write_bit         = 1;

    for (uint64_t i = 0; i < 2; i++) {
        _paging_kernel_page_directory.table[i].pt_base_address  = (((uint64_t)&_paging_kernel_page_tables[i]) >> 12) & 0xFFFFFFFFFFULL;
        _paging_kernel_page_directory.table[i].flag.present_bit = 1;
        _paging_kernel_page_directory.table[i].flag.write_bit   = 1;
    }

    for (uint64_t i = 0; i < 2*PAGE_ENTRY_COUNT; i++) {
        struct PageTableEntry *entry = &_paging_kernel_page_tables[i / PAGE_ENTRY_COUNT].table[i % PAGE_ENTRY_COUNT];
        entry->frame_address     = i;
        entry->flag.present_bit  = 1;
        entry->flag.write_bit    = 1;
    }
}