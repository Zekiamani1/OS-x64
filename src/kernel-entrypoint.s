global start64       ; the entry symbol for ELF
extern kernel_setup ; kernel
extern GDT_DATA
KERNEL_STACK_SIZE equ 2097152     ; size of stack in bytes

section .bss
align 8                              ; align at 8 bytes
kernel_stack:                        ; label points to beginning of memory
    resb KERNEL_STACK_SIZE           ; reserve stack for the kernel

section .text                                  ; start of the text (code) 
start64:                                        ; the loader label (defined as entry point in linker script)
    cli
    mov ax, GDT_DATA ;set up segmentation
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov  esp, kernel_stack + KERNEL_STACK_SIZE ; setup stack register to proper location

    ; enable SSE
    mov rax, cr0
    and rax, ~(1 << 2)        ; CR0.EM = 0
    or rax, (1 << 1)          ; CR0.MP = 1
    mov cr0, rax

    mov rax, cr4
    or rax, (1 << 9)          ; CR4.OSFXSR
    or rax, (1 << 10)         ; CR4.OSXMMEXCPT
    mov cr4, rax

    call kernel_setup
.loop:
    jmp .loop                                  ; loop forever