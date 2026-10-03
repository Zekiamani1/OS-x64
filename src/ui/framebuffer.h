#ifndef _FRAMEBUFFER_H
#define _FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FRAMEBUFFER_MEMORY_OFFSET ((uint8_t*) 0xB8000)
#define CURSOR_PORT_CMD    0x03D4
#define CURSOR_PORT_DATA   0x03D5

#define FRAMEBUFFER_COLOR_BLACK 0x0
#define FRAMEBUFFER_COLOR_WHITE 0xF
#define FRAMEBUFFER_DEFAULT_FG  FRAMEBUFFER_COLOR_WHITE
#define FRAMEBUFFER_DEFAULT_BG  FRAMEBUFFER_COLOR_BLACK
#define FRAMEBUFFER_ATTR(fg, bg) ((((bg) & 0x0F) << 4) | ((fg) & 0x0F))
#define FRAMEBUFFER_ATTR_DEFAULT FRAMEBUFFER_ATTR(FRAMEBUFFER_DEFAULT_FG, FRAMEBUFFER_DEFAULT_BG)

/**
 * Terminal framebuffer
 * Resolution: 80x25
 * Starting at FRAMEBUFFER_MEMORY_OFFSET,
 * - Even number memory: Character, 8-bit
 * - Odd number memory:  Character color lower 4-bit, Background color upper 4-bit
*/
struct FBAttribute
{
    int color;
    int len;
    bool Atcursor;
};

/**
 * Set framebuffer character and color with corresponding parameter values.
 * More details: https://en.wikipedia.org/wiki/BIOS_color_attributes
 *
 * @param row Vertical location (index start 0)
 * @param col Horizontal location (index start 0)
 * @param c   Character
 * @param fg  Foreground / Character color
 * @param bg  Background color
 */
void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg);

/**
 * Set cursor to specified location. Row and column starts from 0
 * 
 * @param r row
 * @param c column
*/
void framebuffer_set_cursor(uint8_t r, uint8_t c);

/**
 * Set all cell in framebuffer character to 0x00 (empty character)
 * and color to FRAMEBUFFER_ATTR_DEFAULT (white character & black background)
 * Extra note: It's allowed to use different color palette for this
 *
 */
void framebuffer_clear(void);

/**
 * Scroll the framebuffer content up by one line.
 * All lines move up, and the bottom line is cleared.
 */
void framebuffer_scroll_up(void);

/**
 * @brief Retrieve the current position of the cursor in the framebuffer.
 *
 * @return The cursor position as a 16-bit unsigned integer.
 */
uint16_t get_cursor_position(void);


/**
 * @brief Write a single character to the framebuffer at the current cursor position.
 *
 * @param c Character to be displayed.
 */
void framebuffer_putchar(char c);


/**
 * @brief Print a buffer of characters to the framebuffer with the specified attributes.
 *
 * @param buf       Null-terminated string to print.
 * @param atttribute Pointer to the attribute descriptor defining color, length, and cursor behavior.
 * @param pos        Starting cursor position for the output.
 */
void puts(char *buf,struct FBAttribute *atttribute, uint16_t pos);
void print(char s[]);
#endif