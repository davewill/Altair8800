
#ifndef _LCDDUE_H_
#define _LCDDUE_H_
#include "config.h"
#include <Arduino.h>


#if defined(__SAM3X8E__) && DAZZLCD==0
#define lcdSetup()
#define dazzler_lcd_update_pictport(v)
#define dazzler_lcd_clear()
#define dazzler_lcd_draw_byte( a,  v)
#define dazzler_lcd_full_redraw()
#define dazzler_lcd_full_redraw(c)
#define dazzler_lcd_update_pictport(v)
#define dazzler_lcd_update_ctrlport(v)
#else
#define ILI9341 0
#define USERA8875  1
#define USELOWLEVELSPI 0 // currently only works with ILI9341

// Use SPI
#define TFT_CS   13

void lcdSetup(void);
void dazzler_lcd_update_pictport(uint8_t v);
void dazzler_lcd_clear(void);
void dazzler_lcd_draw_byte(uint16_t a, byte v);
void dazzler_lcd_full_redraw(boolean colorchangeonly = 0);
void dazzler_lcd_update_pictport(uint8_t v);
void dazzler_lcd_update_ctrlport(uint8_t v);
#endif

#endif
