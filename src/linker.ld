ENTRY(bootstrap)
SECTIONS {
    . = 0x00100000;

    .multiboot ALIGN (0x1000):
    {
        *(.multiboot)        /* GRUB multiboot header */
    }
    .bootstrap ALIGN (0x1000):
    {
        KEEP(*(.bootstrap)) /* the code that turn protected mode to long mode */
    }
    . += 0xFFFF800000000000;
    _linker_kernel_virtual_addr_start  = .; 
    _linker_kernel_physical_addr_start = . - 0xFFFF800000000000; 
    .text ALIGN (0x1000): AT(ADDR(.text) - 0xFFFF800000000000)
    {
        *(.text) /* all text sections from all files */
    }

    .rodata ALIGN (0x1000) : AT(ADDR(.rodata) - 0xFFFF800000000000)
    {
        *(.rodata*)          /* all read-only data sections from all files */
    }

    .data ALIGN (0x1000) : AT(ADDR(.data) - 0xFFFF800000000000)
    {
        *(.data)             /* all data sections from all files */
    }

    .bss ALIGN (0x1000) :   AT(ADDR(.bss) - 0xFFFF800000000000)
    {
        *(COMMON)            /* all COMMON sections from all files */
        *(.bss)              /* all bss sections from all files */
        [a-z\/]*kernel-entrypoint.o(.bss)
        _linker_kernel_stack_top = .;
    }
    /* Linker variable that can be used in kernel */
    _linker_kernel_virtual_addr_end  = .;
    _linker_kernel_physical_addr_end = . - 0xFFFF800000000000;
}