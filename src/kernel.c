#include <stdint.h>
#include <stdbool.h>
#include "kernel-entrypoint.h"
#include "ui/framebuffer.h"
#include "cpu/idt.h"
#include "interrupt/interrupt.h"
#include "driver/keyboard.h"
#include "filesystem/ext2.h"
void kernel_setup(void) {
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    keyboard_state_activate();
    initialize_filesystem_ext2();
    struct EXT2DriverRequest requst={0};
    requst.name="ini.txt";
    requst.name_len=7;
    requst.is_directory=false;
    requst.parent_inode=1;
    requst.buffer_size=100;
    read(requst);
    print(requst.buf);

    char a;
    while (true)
    {
        get_keyboard_buffer(&a,NULL);
        framebuffer_putchar(a);
    }   

}
