;this code will be executed by grub to set up long mode before executing the kernel
BITS 32
global GDT_DATA

EXTERN start64

PML4T_ADDR equ 0x1000 ;PML4T address to be inserted into cr3
PDPT_ADDR equ 0x2000 ;PDPT address to be pointed by PMLT entry
PDT_ADDR equ 0x3000 ;PDT address to be pointed by PDPT entry
PT_ADDR equ 0x4000 ;PT address to be pointed by PDE
SIZEOF_PAGE_TABLE equ 4096

PT_ADDR_MASK equ 0xffffffffff000 ; because the 3 last bit for flags
PT_PRESENT equ 1                 ; marks the entry as in use
PT_READABLE equ 2                ; marks the entry as r/w
HUGE_PAGE   equ 1 << 7        ; PS bit in PDE

ENTRIES_PER_PT equ 512
SIZEOF_PT_ENTRY equ 8
PAGE_SIZE equ 0x1000

CR4_PAE_ENABLE equ 1 << 5
EFER_MSR equ 0xC0000080
EFER_LM_ENABLE equ 1 << 8
CR0_PM_ENABLE equ 1 << 0
CR0_PG_ENABLE equ 1 << 31

;GDT Access bits
PRESENT        equ 1 << 7
NOT_SYS        equ 1 << 4
EXEC           equ 1 << 3
DC             equ 1 << 2
RW             equ 1 << 1
ACCESSED       equ 1 << 0

; GDT Flags bits
GRAN_4K       equ 1 << 7
SZ_32         equ 1 << 6
LONG_MODE     equ 1 << 5

GDT_DATA equ 0x10
GDT_CODE equ 0x08

align 8
GDT:
    .Null:
        dq 0

    .Code:
        dw 0xffff
        dw 0
        db 0
        db PRESENT | NOT_SYS | EXEC | RW
        db GRAN_4K | LONG_MODE | 0xF
        db 0

    .Data:
        dw 0xffff
        dw 0
        db 0
        db PRESENT | NOT_SYS | RW
        db GRAN_4K | SZ_32 | 0xF
        db 0

    .Pointer:
        dw $ - GDT - 1
        dq GDT


MAGIC_NUMBER equ 0x1BADB002
FLAGS        equ 0x0
CHECKSUM     equ -MAGIC_NUMBER
section .multiboot
align 4

    dd MAGIC_NUMBER
    dd FLAGS
    dd CHECKSUM

section .bootstrap
global bootstrap

bootstrap:
    mov eax, cr0
    and eax, ~CR0_PG_ENABLE
    mov cr0, eax ;disable paging

    mov edi, PML4T_ADDR
    mov cr3, edi       ; cr3 lets the CPU know where the page tables are

    xor eax, eax
    mov ecx, SIZEOF_PAGE_TABLE
    rep stosd          ; writes 4 * SIZEOF_PAGE_TABLE bytes, which is enough space
                       ; for the 4 page tables
    mov edi, cr3       ; reset edi back to the beginning of the page table

    mov DWORD [edi], PDPT_ADDR & PT_ADDR_MASK | PT_PRESENT | PT_READABLE

    mov edi, PDPT_ADDR
    mov DWORD [edi], PDT_ADDR & PT_ADDR_MASK | PT_PRESENT | PT_READABLE

    mov edi, PDT_ADDR
    mov ebx, PT_PRESENT | PT_READABLE | HUGE_PAGE
    mov ecx, ENTRIES_PER_PT      ; 1 full page table addresses 2MiB

.SetEntry: ;fill page table
    mov DWORD [edi], ebx
    add ebx, PAGE_SIZE
    add edi, SIZEOF_PT_ENTRY
    loop .SetEntry               ; Set the next entry.

    mov eax, cr4 ;enable PAE
    or eax, CR4_PAE_ENABLE
    mov cr4, eax

    mov ecx, EFER_MSR
    rdmsr
    or eax, EFER_LM_ENABLE
    wrmsr

    mov eax, cr0
    or eax, CR0_PG_ENABLE | CR0_PM_ENABLE   ; ensuring that PM is set will allow for jumping
                                            ; from real mode to compatibility mode directly
    mov cr0, eax
    lgdt [GDT.Pointer]
    jmp GDT_CODE:start64

;thank you https://wiki.osdev.org/Setting_Up_Long_Mode for the reference