#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "framebuffer.h"
#include "../stdlib/string.h"
#include "../cpu/portio.h"

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    FRAMEBUFFER_MEMORY_OFFSET[(row*80+col)*2]= c;
    FRAMEBUFFER_MEMORY_OFFSET[((row*80+col)*2)+1]=(bg << 4) | (fg & 0x0F);
}

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    uint16_t pos= r*80 + c;
    out(0x3d4,0xf);
    out(0x3d5,pos&0xff);
    out(0x3d4,0xe);
    out(0x3d5,(pos>>8)&0xff);

}

void framebuffer_clear(void) {
    for (int i = 0; i < 80*25; i++) {
        FRAMEBUFFER_MEMORY_OFFSET[i*2]= 0x0;
        FRAMEBUFFER_MEMORY_OFFSET[i*2 +1]=FRAMEBUFFER_ATTR_DEFAULT;
    }
}

void framebuffer_scroll_up(void) {
    memmove(FRAMEBUFFER_MEMORY_OFFSET,                     
            FRAMEBUFFER_MEMORY_OFFSET + (80 * 2),         
            24 * 80 * 2);                                 
    
    uint8_t *last_line = FRAMEBUFFER_MEMORY_OFFSET + (24 * 80 * 2);
    
    memset(last_line, 0x20, 160); 
    for (int i = 0; i < 160; i += 2) {
        last_line[i] = 0x20;
        last_line[i + 1] = FRAMEBUFFER_ATTR_DEFAULT;
    }
}

uint16_t get_cursor_position(void) {
    uint16_t pos = 0;

    out(0x3D4, 0x0F); 
    pos |= in(0x3D5);          

    out(0x3D4, 0x0E);
    pos |= ((uint16_t)in(0x3D5)) << 8;  

    return pos;
}

int get_cursor_x(uint16_t pos) {
    return pos%80;
}

int get_cursor_y(uint16_t pos) {
    return pos/80;
}

void framebuffer_putchar(char c) {
    struct FBAttribute attr = {
        .color = FRAMEBUFFER_ATTR_DEFAULT,
        .len = 1,
        .Atcursor = true,
    };
    puts(&c, &attr, get_cursor_position());
}

void print(char s[]) {
    int i=0;
    uint16_t pos=get_cursor_position();
    int x=get_cursor_x(pos);
    int y=get_cursor_y(pos);
    while(s[i]!=0x0) {
        if (s[i]=='\n') {
            y++;
            x=0;
            // Handle scrolling when reaching bottom of screen
            if (y >= 25) {
                framebuffer_scroll_up();
                y = 24; // Stay on last row
            }
            framebuffer_set_cursor(y,x);
            i++;
        } else{
            // Handle line wrapping
            if (x >= 80) {
                y++;
                x = 0;
                // Handle scrolling when reaching bottom of screen
                if (y >= 25) {
                    framebuffer_scroll_up();
                    y = 24; // Stay on last row
                }
            }
            framebuffer_write(y,x,s[i],FRAMEBUFFER_DEFAULT_FG,FRAMEBUFFER_DEFAULT_BG);
            x++;
            framebuffer_set_cursor(y,x);
            i++;
        }
    }
}

void puts(char *buf,struct FBAttribute *atttribute, uint16_t pos) {
    const uint16_t FB_WIDTH = 80;
    const uint16_t FB_CELLS = 80 * 25;

    uint16_t cursor = atttribute->Atcursor ? get_cursor_position() : pos;
    uint8_t color = (uint8_t)(atttribute->color & 0xFF);
    uint8_t fg = color & 0x0F;
    uint8_t bg = (color >> 4) & 0x0F;

    for (int i = 0; i < atttribute->len; i++) {
        char ch = buf[i];
        if (ch == 0) {
            break;
        }

        if (ch == '\b') {
            if (cursor > 0) {
                cursor--;
                framebuffer_write(cursor / FB_WIDTH, cursor % FB_WIDTH, 0x20, fg, bg);
            }
            continue;
        }

        if (ch == '\n') {
            uint16_t offset = cursor % FB_WIDTH;
            cursor += (uint16_t)(FB_WIDTH - offset);
            if (cursor >= FB_CELLS) {
                framebuffer_scroll_up();
                cursor -= FB_WIDTH;
            }
            continue;
        }

        framebuffer_write(cursor / FB_WIDTH, cursor % FB_WIDTH, ch, fg, bg);
        cursor++;
        if (cursor >= FB_CELLS) {
            framebuffer_scroll_up();
            cursor -= FB_WIDTH;
        }
    }

    if (atttribute->Atcursor) {
        framebuffer_set_cursor(cursor / FB_WIDTH, cursor % FB_WIDTH);
    }
}