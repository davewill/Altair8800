
#ifndef _LCDDUE_H_
#define _LCDDUE_H_


#if DAZZLCD>0
#ifndef __SAM3X8E__
#error DAZZLCD cannot be used except on Arduino Due
#endif

#define ILI9341 0
#define USERA8875  1
#define USELOWLEVELSPI 0 // currently only works with ILI9341

// Use SPI
#define TFT_CS   13

enum lcdLayout 
{
    LAYOUT_DAZZ,
    LAYOUT_TERM,
    LAYOUT_DAZZ_MAIN,
    LAYOUT_TERM_MAIN,
    NUMLAYOUTS
};

void lcd_setup(void);
void lcd_set_screen_config(enum lcdLayout config);

void lcd_term_write(const char *buf, size_t n);
void lcd_term_full_redraw();

byte lcd_read_joy(uint8_t port);

void dazzler_lcd_update_pictport(uint8_t v);
void dazzler_lcd_clear(void);
void dazzler_lcd_draw_byte(uint8_t buffer_flag, uint16_t a, byte v);
void dazzler_lcd_full_redraw(uint8_t buffer_flag, uint16_t addr, boolean colorchangeonly = 0);
void dazzler_lcd_update_pictport(uint8_t v);
void dazzler_lcd_update_ctrlport(uint8_t v);

void dazzler_lcd_setlayer(uint8_t buffer_flag);
#endif

#if USETOUCH>0
void lcdCheckTouch(void);
#endif

#endif
