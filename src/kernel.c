#include <stdint.h>
#include <stdbool.h>
#include "kernel-entrypoint.h"
#include "ui/framebuffer.h"
#include "cpu/idt.h"
#include "interrupt/interrupt.h"
#include "driver/keyboard.h"
void kernel_setup(void) {
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    keyboard_state_activate();
    char a;
    while (true)
    {
        get_keyboard_buffer(&a,NULL);
        framebuffer_putchar(a);
    }   

}
