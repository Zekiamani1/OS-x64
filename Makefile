# Compiler & linker
ASM           = nasm
LIN           = ld
CC            = gcc

# Directory
SOURCE_FOLDER = src
OUTPUT_FOLDER = bin
ISO_NAME      = OS
DISK_NAME     = storage

# Flags
WARNING_CFLAG = -Wall -Wextra -Werror
DEBUG_CFLAG   = -fshort-wchar -g
STRIP_CFLAG   = -nostdlib -fno-stack-protector -nostartfiles -nodefaultlibs -ffreestanding
CFLAGS        = $(DEBUG_CFLAG) $(WARNING_CFLAG) $(STRIP_CFLAG) -m64 -c -I$(SOURCE_FOLDER)
AFLAGS        = -f elf64 -g -F dwarf
LFLAGS        = -T $(SOURCE_FOLDER)/linker.ld -melf_x86_64
DISK_NAME      = storage


.PHONY: all build run clean iso kernel

all: iso
disk:
	@qemu-img create -f raw $(OUTPUT_FOLDER)/$(DISK_NAME).bin 4M

run: iso
	# By default we don't enable QEMU's gdb server (which binds TCP port 1234).
	# To enable GDB server set QEMU_GDB_FLAGS, e.g.:
	#   make run QEMU_GDB_FLAGS="-s -S"
	@qemu-system-x86_64 -s -drive file=bin/storage.bin,format=raw,if=ide,index=0,media=disk -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso

build: iso

$(OUTPUT_FOLDER):
	@mkdir -p $(OUTPUT_FOLDER)

kernel: | $(OUTPUT_FOLDER)
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/longmode-entrypoint.s -o $(OUTPUT_FOLDER)/longmode-entrypoint.o
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/kernel-entrypoint.s -o $(OUTPUT_FOLDER)/kernel-entrypoint.o
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/interrupt/intsetup.s -o $(OUTPUT_FOLDER)/intsetup.o
	@for file in $$(find $(SOURCE_FOLDER) -name "*.c" -not -path "$(SOURCE_FOLDER)/external/*" -not -path "$(SOURCE_FOLDER)/usermode/*"); do $(CC) $(CFLAGS) $$file -o $(OUTPUT_FOLDER)/$$(basename $$file .c).o; done
	@$(LIN) $(LFLAGS) $(OUTPUT_FOLDER)/*.o -o $(OUTPUT_FOLDER)/kernel
	@echo Linking object files and generate elf64...
	@rm -f *.o

$(OUTPUT_FOLDER)/$(ISO_NAME).iso: kernel
	@mkdir -p $(OUTPUT_FOLDER)/iso/boot/grub
	@cp $(OUTPUT_FOLDER)/kernel $(OUTPUT_FOLDER)/iso/boot/
	@cp $(SOURCE_FOLDER)/grub.cfg $(OUTPUT_FOLDER)/iso/boot/grub/
	@grub-mkrescue -o $(OUTPUT_FOLDER)/$(ISO_NAME).iso $(OUTPUT_FOLDER)/iso
	
iso: $(OUTPUT_FOLDER)/$(ISO_NAME).iso

clean:
	rm -rf *.o *.iso \
		$(OUTPUT_FOLDER)/kernel \
		$(OUTPUT_FOLDER)/*.o \
		$(OUTPUT_FOLDER)/$(ISO_NAME).iso \
		$(OUTPUT_FOLDER)/iso
