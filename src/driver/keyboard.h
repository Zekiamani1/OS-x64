#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define EXT_SCANCODE_UP        0x48
#define EXT_SCANCODE_DOWN      0x50
#define EXT_SCANCODE_LEFT      0x4B
#define EXT_SCANCODE_RIGHT     0x4D

#define KEYBOARD_DATA_PORT     0x60
#define EXTENDED_SCANCODE_BYTE 0xE0

#define KEYBOARD_KEYCODE_ARROW_UP      0x80
#define KEYBOARD_KEYCODE_ARROW_DOWN    0x81
#define KEYBOARD_KEYCODE_ARROW_LEFT    0x82
#define KEYBOARD_KEYCODE_ARROW_RIGHT   0x83
#define KEYBOARD_KEYCODE_HOME          0x84
#define KEYBOARD_KEYCODE_END           0x85
#define KEYBOARD_KEYCODE_PAGE_UP       0x86
#define KEYBOARD_KEYCODE_PAGE_DOWN     0x87
#define KEYBOARD_KEYCODE_INSERT        0x88
#define KEYBOARD_KEYCODE_DELETE        0x89
#define KEYBOARD_KEYCODE_F1            0x90
#define KEYBOARD_KEYCODE_F2            0x91
#define KEYBOARD_KEYCODE_F3            0x92
#define KEYBOARD_KEYCODE_F4            0x93
#define KEYBOARD_KEYCODE_F5            0x94
#define KEYBOARD_KEYCODE_F6            0x95
#define KEYBOARD_KEYCODE_F7            0x96
#define KEYBOARD_KEYCODE_F8            0x97
#define KEYBOARD_KEYCODE_F9            0x98
#define KEYBOARD_KEYCODE_F10           0x99
#define KEYBOARD_KEYCODE_F11           0x9A
#define KEYBOARD_KEYCODE_F12           0x9B
#define KEYBOARD_KEYCODE_NUM_LOCK      0xA0
#define KEYBOARD_KEYCODE_CAPS_LOCK     0xA1
#define KEYBOARD_KEYCODE_SCROLL_LOCK   0xA2
#define KEYBOARD_KEYCODE_PRINT_SCREEN  0xA3
#define KEYBOARD_KEYCODE_PAUSE_BREAK   0xA4
#define KEYBOARD_KEYCODE_LEFT_GUI      0xA5
#define KEYBOARD_KEYCODE_RIGHT_GUI     0xA6
#define KEYBOARD_KEYCODE_MENU          0xA7

#define KEYBOARD_MODIFIER_LEFT_SHIFT   (1u << 0)
#define KEYBOARD_MODIFIER_RIGHT_SHIFT  (1u << 1)
#define KEYBOARD_MODIFIER_LEFT_CTRL    (1u << 2)
#define KEYBOARD_MODIFIER_RIGHT_CTRL   (1u << 3)
#define KEYBOARD_MODIFIER_LEFT_ALT     (1u << 4)
#define KEYBOARD_MODIFIER_RIGHT_ALT    (1u << 5)
#define KEYBOARD_MODIFIER_CAPS_LOCK    (1u << 6)
#define KEYBOARD_MODIFIER_NUM_LOCK     (1u << 7)
#define KEYBOARD_MODIFIER_SCROLL_LOCK  (1u << 8)

/**
 * KeyboardDriverState - Contain all driver states
 * 
 * @param read_extended_mode Optional, can be used for signaling next read is extended scancode (ex. arrow keys)
 * @param keyboard_input_on  Indicate whether keyboard ISR is activated or not
 * @param keyboard_buffer    Stores ASCII input or one of the KEYBOARD_KEYCODE_* values
 */
struct KeyboardDriverState {
    bool read_extended_mode;
    bool keyboard_input_on;
    bool left_shift;
    bool right_shift;
    bool left_ctrl;
    bool right_ctrl;
    bool left_alt;
    bool right_alt;
    bool caps_lock;
    bool num_lock;
    bool scroll_lock;
    uint16_t modifier_state;
    uint8_t pause_discard;
    char keyboard_buffer;
} __attribute((packed));

/* -- Driver Interfaces -- */

// Activate keyboard ISR / start listen keyboard & save to buffer
void keyboard_state_activate(void);

// Deactivate keyboard ISR / stop listening keyboard interrupt
void keyboard_state_deactivate(void);

// Get keyboard buffer value and flush the buffer - @param buf Pointer to char buffer
void get_keyboard_buffer(char *buf, uint16_t *modifiers);

/* -- Keyboard Interrupt Service Routine -- */

/**
 * Handling keyboard interrupt & process scancodes into ASCII character.
 * Will start listen and process keyboard scancode if keyboard_input_on.
 */
void keyboard_isr(void);

#endif