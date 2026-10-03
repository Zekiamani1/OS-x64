#include "keyboard.h"
#include "../cpu/portio.h"

#define KEY_RELEASE_FLAG 0x80
#define SCANCODE_MASK 0x7F

static const char keyboard_scancode_1_to_ascii_map[128] = {
    [0x01] = 0x1B,
    [0x02] = '1',  [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0', [0x0C] = '-',  [0x0D] = '=', [0x0E] = '\b',
    [0x0F] = '\t', [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',  [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n',
    [0x1E] = 'a',  [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`', [0x2B] = '\\',
    [0x2C] = 'z',  [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',

    [0x37] = '*', [0x39] = ' ',
    [0x47] = '7', [0x48] = '8', [0x49] = '9', [0x4A] = '-',
    [0x4B] = '4', [0x4C] = '5', [0x4D] = '6', [0x4E] = '+',
    [0x4F] = '1', [0x50] = '2', [0x51] = '3',
    [0x52] = '0', [0x53] = '.',
};

static const char keyboard_scancode_shift_to_ascii_map[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x1A] = '{', [0x1B] = '}', [0x27] = ':', [0x28] = '"', [0x29] = '~', [0x2B] = '|', [0x33] = '<', [0x34] = '>', [0x35] = '?',
};

static const uint8_t keypad_navigation_map[128] = {
    [0x47] = KEYBOARD_KEYCODE_HOME,
    [0x48] = KEYBOARD_KEYCODE_ARROW_UP,
    [0x49] = KEYBOARD_KEYCODE_PAGE_UP,
    [0x4B] = KEYBOARD_KEYCODE_ARROW_LEFT,
    [0x4D] = KEYBOARD_KEYCODE_ARROW_RIGHT,
    [0x4F] = KEYBOARD_KEYCODE_END,
    [0x50] = KEYBOARD_KEYCODE_ARROW_DOWN,
    [0x51] = KEYBOARD_KEYCODE_PAGE_DOWN,
    [0x52] = KEYBOARD_KEYCODE_INSERT,
    [0x53] = KEYBOARD_KEYCODE_DELETE,
};

static const uint8_t function_key_map[128] = {
    [0x3B] = KEYBOARD_KEYCODE_F1,
    [0x3C] = KEYBOARD_KEYCODE_F2,
    [0x3D] = KEYBOARD_KEYCODE_F3,
    [0x3E] = KEYBOARD_KEYCODE_F4,
    [0x3F] = KEYBOARD_KEYCODE_F5,
    [0x40] = KEYBOARD_KEYCODE_F6,
    [0x41] = KEYBOARD_KEYCODE_F7,
    [0x42] = KEYBOARD_KEYCODE_F8,
    [0x43] = KEYBOARD_KEYCODE_F9,
    [0x44] = KEYBOARD_KEYCODE_F10,
    [0x57] = KEYBOARD_KEYCODE_F11,
    [0x58] = KEYBOARD_KEYCODE_F12,
};

static const uint8_t extended_navigation_map[128] = {
    [0x47] = KEYBOARD_KEYCODE_HOME,
    [0x48] = KEYBOARD_KEYCODE_ARROW_UP,
    [0x49] = KEYBOARD_KEYCODE_PAGE_UP,
    [0x4B] = KEYBOARD_KEYCODE_ARROW_LEFT,
    [0x4D] = KEYBOARD_KEYCODE_ARROW_RIGHT,
    [0x4F] = KEYBOARD_KEYCODE_END,
    [0x50] = KEYBOARD_KEYCODE_ARROW_DOWN,
    [0x51] = KEYBOARD_KEYCODE_PAGE_DOWN,
    [0x52] = KEYBOARD_KEYCODE_INSERT,
    [0x53] = KEYBOARD_KEYCODE_DELETE,
};

static const char extended_ascii_map[128] = {
    [0x1C] = '\n', // keypad enter
    [0x35] = '/',  // keypad slash
};

static struct KeyboardDriverState keyboard = {
    .num_lock = true,
    .modifier_state = KEYBOARD_MODIFIER_NUM_LOCK,
};

static inline bool shift_active(void) {
    return keyboard.left_shift || keyboard.right_shift;
}

static inline bool ctrl_active(void) {
    return keyboard.left_ctrl || keyboard.right_ctrl;
}

static inline void queue_ascii(char ch) {
    keyboard.keyboard_buffer = ch;
}

static inline void queue_special(uint8_t code) {
    keyboard.keyboard_buffer = (char)code;
}

static void update_modifier_flag(uint16_t flag, bool active) {
    if (active) {
        keyboard.modifier_state |= flag;
    }
    else {
      keyboard.modifier_state &= (uint16_t)~flag;
    }
}

static bool is_lower_letter(char ch) {
    return ch >= 'a' && ch <= 'z';
}

static char translate_printable(uint8_t scancode) {
    bool ctrl = ctrl_active();

    if (ctrl) {
        switch (scancode) {
            case 0x1A:
               return 0x1B; // '['
            case 0x2B:
                return 0x1C; // '\'
            case 0x1B:
                return 0x1D; // ']'
            case 0x07:
                return 0x1E; // 6 -> ^
            case 0x0C:
                return 0x1F; // - -> _
            }
    }

    char base = keyboard_scancode_1_to_ascii_map[scancode];
    if (base == 0) {
        return 0;
    }

    if (is_lower_letter(base)) {
        if (keyboard.caps_lock ^ shift_active()) {
            base = (char)(base - ('a' - 'A'));
        }
        if (ctrl) {
            base = (char)(base - 'A' + 1);
        }
        return base;
    }

    if (shift_active()) {
        char shifted = keyboard_scancode_shift_to_ascii_map[scancode];
        if (shifted != 0) {
            base = shifted;
        }
    }

    if (ctrl && base >= 'A' && base <= 'Z') {
        base = (char)(base - 'A' + 1);
    }

    return base;
}

static void handle_standard_scancode(uint8_t raw) {
    bool release = (raw & KEY_RELEASE_FLAG) != 0;
    uint8_t scancode = raw & SCANCODE_MASK;

    switch (scancode) {
    case 0x2A:
        keyboard.left_shift = !release;
        update_modifier_flag(KEYBOARD_MODIFIER_LEFT_SHIFT, keyboard.left_shift);
        return;
    case 0x36:
        keyboard.right_shift = !release;
        update_modifier_flag(KEYBOARD_MODIFIER_RIGHT_SHIFT, keyboard.right_shift);
        return;
    case 0x1D:
        keyboard.left_ctrl = !release;
        update_modifier_flag(KEYBOARD_MODIFIER_LEFT_CTRL, keyboard.left_ctrl);
        return;
    case 0x38:
        keyboard.left_alt = !release;
        update_modifier_flag(KEYBOARD_MODIFIER_LEFT_ALT, keyboard.left_alt);
        return;
    case 0x3A:
        if (!release){
            keyboard.caps_lock = !keyboard.caps_lock;
            update_modifier_flag(KEYBOARD_MODIFIER_CAPS_LOCK, keyboard.caps_lock);
            queue_special(KEYBOARD_KEYCODE_CAPS_LOCK);
        }
        return;
    case 0x45:
        if (!release){
            keyboard.num_lock = !keyboard.num_lock;
            update_modifier_flag(KEYBOARD_MODIFIER_NUM_LOCK, keyboard.num_lock);
            queue_special(KEYBOARD_KEYCODE_NUM_LOCK);
        }
        return;
    case 0x46:
        if (!release) {
            keyboard.scroll_lock = !keyboard.scroll_lock;
            update_modifier_flag(KEYBOARD_MODIFIER_SCROLL_LOCK, keyboard.scroll_lock);
            queue_special(KEYBOARD_KEYCODE_SCROLL_LOCK);
        }
        return;
    }

    if (release){
        return;
    }

    if (!keyboard.num_lock) {
        uint8_t nav_code = keypad_navigation_map[scancode];
        if (nav_code != 0) {
            queue_special(nav_code);
            return;
        }
    }

    uint8_t fn_code = function_key_map[scancode];
    if (fn_code != 0) {
        queue_special(fn_code);
        return;
    }

    char printable = translate_printable(scancode);
    if (printable != 0){
        queue_ascii(printable);
    }
}

static void handle_extended_scancode(uint8_t raw) {
    bool release = (raw & KEY_RELEASE_FLAG) != 0;
    uint8_t scancode = raw & SCANCODE_MASK;

    switch (scancode) {
    case 0x1D:
        keyboard.right_ctrl = !release;
        update_modifier_flag(KEYBOARD_MODIFIER_RIGHT_CTRL, keyboard.right_ctrl);
        return;
    case 0x38:
        keyboard.right_alt = !release;
        update_modifier_flag(KEYBOARD_MODIFIER_RIGHT_ALT, keyboard.right_alt);
        return;
    }

    if (release) {
        return;
    }

    if (scancode == 0x37) {
        queue_special(KEYBOARD_KEYCODE_PRINT_SCREEN);
        return;
    }
    
    if (scancode == 0x5B) {
        queue_special(KEYBOARD_KEYCODE_LEFT_GUI);
        return;
    }
    if (scancode == 0x5C) {
        queue_special(KEYBOARD_KEYCODE_RIGHT_GUI);
        return;
    }
    if (scancode == 0x5D) {
        queue_special(KEYBOARD_KEYCODE_MENU);
        return;
    }

    uint8_t nav_code = extended_navigation_map[scancode];
    if (nav_code != 0) {
        queue_special(nav_code);
        return;
    }

    char ascii = extended_ascii_map[scancode];
    if (ascii != 0) {
        queue_ascii(ascii);
    }
}

void keyboard_state_activate(void) {
    keyboard.keyboard_input_on = true;
    keyboard.read_extended_mode = false;
    keyboard.keyboard_buffer = 0;
    keyboard.pause_discard = 0;
}

void keyboard_state_deactivate(void) {
    keyboard.keyboard_input_on = false;
    keyboard.keyboard_buffer = 0;
}

void get_keyboard_buffer(char *buf, uint16_t *modifiers) {
    if (buf != NULL) {
        *buf = keyboard.keyboard_buffer;
        keyboard.keyboard_buffer = 0;
    }
    if (modifiers != NULL) {
        *modifiers = keyboard.modifier_state;
    }
}

void keyboard_isr(void) {
    uint8_t scancode = in(KEYBOARD_DATA_PORT);
    if (!keyboard.keyboard_input_on) {
        return;
    }

    if (keyboard.pause_discard > 0) {
        keyboard.pause_discard--;
        return;
    }

    if (scancode == 0xE1) {
        keyboard.pause_discard = 5;
        queue_special(KEYBOARD_KEYCODE_PAUSE_BREAK);
        return;
    }

    if (scancode == EXTENDED_SCANCODE_BYTE) {
        keyboard.read_extended_mode = true;
        return;
    }

    if (keyboard.read_extended_mode) {
        handle_extended_scancode(scancode);
        keyboard.read_extended_mode = false;
        return;
    }

    handle_standard_scancode(scancode);
}