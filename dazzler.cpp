// -----------------------------------------------------------------------------
// Altair 8800 Simulator
// Copyright (C) 2017 David Hansel
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#include "dazzler.h"
#include "mem.h"
#include "cpucore.h"
#include "host.h"
#include "serial.h"
#include "timer.h"
#include "numsys.h"
#include <malloc.h>

#if USE_DAZZLER==0

void dazzler_out_ctrl(byte v) {}
void dazzler_out_pict(byte v) {}
void dazzler_out_dac(byte port, byte v) {}
byte dazzler_in(byte port) { return 0xff; }
void dazzler_set_iface(byte iface) {}
byte dazzler_get_iface() { return 0xff; }
void dazzler_setup() {}

#else

#define DAZZLER_COMPUTER_VERSION 0x02

#define DAZ_MEMBYTE   0x10
#define DAZ_FULLFRAME 0x20
#define DAZ_CTRL      0x30
#define DAZ_CTRLPIC   0x40
#define DAZ_DAC       0x50
#define DAZ_VERSION   0xF0

#define DAZ_JOY1      0x10
#define DAZ_JOY2      0x20
#define DAZ_KEY       0x30
#define DAZ_VSYNC     0x40

#define BUFFER1       0x00
#define BUFFER2       0x08

#define FEAT_VIDEO    0x01
#define FEAT_JOYSTICK 0x02
#define FEAT_DUAL_BUF 0x04
#define FEAT_VSYNC    0x08
#define FEAT_DAC      0x10
#define FEAT_KEYBAORD 0x20
#define FEAT_FRAMEBUF 0x40

#define DEBUGLVL 0

byte dazzler_iface = 0xff;
int  dazzler_client_version = -1;
uint16_t dazzler_client_features = 0;
uint32_t dazzler_vsync_cycles = 0;

volatile byte ctrlport = 0x00;
volatile byte pictport = 0x00;

uint16_t dazzler_mem_addr1, dazzler_mem_addr2, dazzler_mem_start, dazzler_mem_end, dazzler_mem_size;
volatile byte d7a_port[5] = {0xff, 0x00, 0x00, 0x00, 0x00};

#if DAZZLCD>0
#include "SPI.h"
#include "Adafruit_GFX.h"

#define ILI9341 1
#define USELOWLEVELSPI 1 // currently only works with ILI9341

// Use SPI
#define TFT_CS   13
#define TFT_DC   9

#if ILI9341>0
#include "Adafruit_ILI9341.h"

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

static uint16_t
  pixelWidth  = ILI9341_TFTWIDTH,  // TFT dimensions
  pixelHeight = ILI9341_TFTHEIGHT;
typedef uint16_t pixel_t;
volatile pixel_t pictcolor = 0xffff;
#endif

#define TFT_ROTATION 1
#define TOPBAR 0
#define SIDEBAR 0

#if (TFT_ROTATION & 0x01) == 1
static uint16_t
  lcdOffsetX = 0,
  lcdOffsetY = 0,
  lcdHeight = pixelWidth,
  lcdWidth = min(pixelWidth, pixelHeight-SIDEBAR);
  #else
static uint16_t
  lcdOffsetX = 0,
  lcdOffsetY = TOPBAR,
  lcdHeight = min(pixelWidth, pixelHeight-TOPBAR),
  lcdWidth = pixelWidth;
#endif

uint8_t dazzres;
#define scaleX(dazzX) (lcdOffsetX+((dazzX)*lcdWidth/dazzres)) // ((256/dazzres)*(dazzX)*240/256)
#define scaleY(dazzY) (lcdOffsetY+((dazzY)*lcdHeight/dazzres)) // ((256/dazzres)*(dazzY)*240/256)

#if ILI9341>0
static pixel_t tftColor(int8_t v)
{
  if (pictport & 0x10)
  {    
  uint8_t i = v & 0x08;
  return tft.color565(((v & 0x01) ? 0xff : 0) / (i ? 1 : 3),
                      ((v & 0x02) ? 0xff : 0) / (i ? 1 : 3),
                      ((v & 0x04) ? 0xff : 0) / (i ? 1 : 3));
  }  
  else
  {
    uint8_t gray = ((v&0x0f) << 4) | ((v&0x0f));
    return tft.color565(gray, gray, gray);
  }
}
#endif

void getXY(uint16_t index, uint8_t &x, uint8_t &y)
{
  uint8_t npix = (pictport & 0x40) ? 4 : 2;
  
  if (!(pictport & 0x20))
  {
    x = npix*(index % (dazzres/npix));
    y = (index / (dazzres/npix));
  }
  else
  {
    uint16_t qindex = index % 512;
  
    x = npix*(qindex % (dazzres/(npix*2)));
    y = (qindex / (dazzres/(npix*2)));

    // Each byte is two lines tall
    if (pictport & 0x40)  y *= 2;
    
    if (index < 512) {
      // X and Y are right
    }
    else if (index < 1024) {
      x = x + dazzres/2;
    }
    else if (index < 1536) {
      y = y + dazzres/2;
    }
    else {
      x = x + dazzres/2;
      y = y + dazzres/2;
    }
  }
}

static void dazzler_lcd_draw_byte_xy(uint8_t x, uint8_t y, byte v, boolean colorchangeonly = 0)
{
    SPIGuard spi;

#if DEBUGLVL>2
  if (x < 5 && y < 5)
    printf("dazzler_lcd_draw_byte_xy(%u, %u)\n", x, y);
#endif
  if (!(ctrlport & 0x80))
    return;
      
    if (!(pictport & 0x40))
    {
      if (v == 0 || v == 0xff)
      {
        tft.fillRect(scaleX(x), scaleY(y), scaleX(x+2)-scaleX(x), scaleY(y+1)-scaleY(y), tftColor(v&0x0F));
      }
      else
      {
        tft.fillRect(scaleX(x), scaleY(y), scaleX(x+1)-scaleX(x), scaleY(y+1)-scaleY(y), tftColor(v&0x0F));
        tft.fillRect(scaleX(x+1), scaleY(y), scaleX(x+2)-scaleX(x+1), scaleY(y+1)-scaleY(y), tftColor(v>>4));
      }
    }
    else
    {
      if (v == 0 || v == 0xff)
      {
          uint16_t c = (v & 0x01) ? pictcolor : 0;
          if (!colorchangeonly || c != 0)
          {
            tft.fillRect(scaleX(x), scaleY(y), scaleX(x+4)-scaleX(x), scaleY(y+2)-scaleY(y), c);
#if DEBUGLVL>2
  if (x < 5 && y < 5)
    printf("tft.fillRect(%u, %u, %04x)\r\n", scaleX(x), scaleY(y), (unsigned int) c);
#endif
          }
      }
      else
      {
        for (int i = 0; i < 2; i++)
        {
          uint16_t c = (v & (0x01 << 4*i)) ? pictcolor : 0;
          if (!colorchangeonly || c != 0)
          {
            tft.fillRect(scaleX(x+2*i), scaleY(y), scaleX(x+2*i+1)-scaleX(x+2*i), scaleY(y+1)-scaleY(y), c);
#if DEBUGLVL>2
  if (x < 5 && y < 5)
    printf("tft.fillRect(%u, %u, %04x)\r\n", scaleX(x+2*i), scaleY(y), (unsigned int) c);
#endif
          }

          c = (v & (0x02 << 4*i)) ? pictcolor : 0;
          if (!colorchangeonly || c != 0)
            tft.fillRect(scaleX(x+2*i+1), scaleY(y), scaleX(x+2*i+2)-scaleX(x+2*i+1), scaleY(y+1)-scaleY(y), c);

          c = (v & (0x04 << 4*i)) ? pictcolor : 0;
          if (!colorchangeonly || c != 0)
            tft.fillRect(scaleX(x+2*i), scaleY(y+1), scaleX(x+2*i+1)-scaleX(x+2*i), scaleY(y+2)-scaleY(y+1), c);

          c = (v & (0x08 << 4*i)) ? pictcolor : 0;
          if (!colorchangeonly || c != 0)
            tft.fillRect(scaleX(x+2*i+1), scaleY(y+1), scaleX(x+2*i+2)-scaleX(x+2*i+1), scaleY(y+2)-scaleY(y+1), c);
        }
      }
#if DEBUGLVL>3
printf("tft.drawPixel(%u, %u, %04x)\r\n", x, y, (unsigned int) v);
#endif
    }
}

static void dazzler_lcd_draw_byte(uint16_t a, byte v)
{
    uint16_t baseaddr = (ctrlport & 0x7f) << 9;
    int32_t index = (int32_t) a-baseaddr;
    uint8_t x, y;

  if (!(ctrlport & 0x80))
  {
    return;
  }
      
#if DEBUGLVL>2
  if (faddr < 5)
    printf("dazzler_lcd_draw_byte(%u, %u), faddr = %u\n", a, v, faddr);
#endif
    if ((ctrlport & 0x80) && index >= 0 && index < ((pictport & 0x20) ? 2048 : 512))
    {
      getXY(index, x, y);

      dazzler_lcd_draw_byte_xy(x, y, v);
    }
}

#if USELOWLEVELSPI>0 && ILI9341>0
static void dazzler_lcd_write_pixels(uint8_t x, uint8_t y, uint16_t addr, pixel_t *pixmap)
{
  uint8_t npix = (pictport & 0x40) ? 4 : 2;
  uint16_t i, j, pmapindex = 0;
  #if DEBUGLVL>2
  printf("dazzler_lcd_draw_row(%u, %u, %04x)\r\n", x, y, addr);
  #endif

  if (!(pictport & 0x40))
  {
    for (i = 0; i < 32; i++) 
    {
      if ((pictport & 0x20) && i == 16)
        addr += 512-16;

      pixel_t c = tftColor(Mem[addr+i] & 0x0F);
      for (j = scaleX(x+i*npix); j < scaleX(x+i*npix+1); j++)
      {
        tft.SPI_WRITE16(pixmap[pmapindex++] = c);
      }

      c = tftColor(Mem[addr+i] >> 4);
      for (;j < scaleX(x+i*npix+2); j++)
      {
        tft.SPI_WRITE16(pixmap[pmapindex++] = c);
      }
    }
  }
  else
  {
    uint8_t odd = y & 0x01;

    for (i = 0; i < 32; i++)
    {
      if ((pictport & 0x20) && i == 16)
        addr += 512-16;

      pixel_t c = (Mem[addr+i] & (0x01 << 2*odd)) ? pictcolor : 0;
      for (j = scaleX(x+i*npix); j < scaleX(x+i*npix+1); j++)
      {
        tft.SPI_WRITE16(pixmap[pmapindex++] = c);
      }

      c = (Mem[addr+i] & (0x02 << 2*odd)) ? pictcolor : 0;
      for (;j < scaleX(x+i*npix+2); j++)
      {
        tft.SPI_WRITE16(pixmap[pmapindex++] = c);
      }

      c = (Mem[addr+i] & (0x10 << 2*odd)) ? pictcolor : 0;
      for (;j < scaleX(x+i*npix+3); j++)
      {
        tft.SPI_WRITE16(pixmap[pmapindex++] = c);
      }

      c = (Mem[addr+i] & (0x20 << 2*odd)) ? pictcolor : 0;
      for (;j < scaleX(x+i*npix+4); j++)
      {
        tft.SPI_WRITE16(pixmap[pmapindex++] = c);
      }
    }
  }
  
  for (i = scaleY(y)+1; i < scaleY(y+1); i++)
  {
    tft.writePixels(pixmap, pmapindex);
    #if DEBUGLVL>3
    printf("tft.drawRGBBitmap(%u, %u, pixmap, %u, 1)\r\n", scaleX(x), i+scaleY(y), pmapindex);
    #endif
  }
}

static void dazzler_lcd_full_redraw()
{
  #if DEBUGLVL>1
  printf("dazzler_lcd_full_draw()\n");
  #endif

  if (!(ctrlport & 0x80))
  {
    #if DEBUGLVL>1
    printf("dazzler_lcd_full_draw() Aborted\n");
    #endif

    return;
  }
      
  uint8_t npix = (pictport & 0x40) ? 4 : 2;
  int addr = (ctrlport & 0x7f) << 9;
  uint8_t x, y;

  SPIGuard spi;

  pixel_t *pixmap = (uint16_t *) malloc(lcdWidth*sizeof(pixel_t));
  if (pixmap == NULL)
    return;

  tft.startWrite();
  tft.setAddrWindow(lcdOffsetX, lcdOffsetY, lcdWidth, lcdHeight); // Clipped area
  for (y = 0; y < dazzres; y++) 
  {
    if (pictport & 0x20)
    {
      if (y == dazzres/2)
        addr += 512;
    }

    dazzler_lcd_write_pixels(0, y, addr, pixmap);
    if (pictport & 0x40) 
    {
      y++;
      dazzler_lcd_write_pixels(0, y, addr, pixmap);
    }

    if (pictport & 0x20)
    {
      addr += dazzres/npix/2;
    }
    else
    {
      addr += dazzres/npix;
    }

  }
  tft.endWrite();

  free(pixmap);
}
#else
static void dazzler_lcd_draw_row(uint8_t y, uint16_t addr, pixel_t *pixmap)
{
  uint8_t npix = (pictport & 0x40) ? 4 : 2;
  uint16_t i, j, pmapindex = 0;
  SPIGuard spi;

  #if DEBUGLVL>2
  printf("dazzler_lcd_draw_row(%u, %u, %04x)\r\n", x, y, addr);
  #endif

  if (!(pictport & 0x40))
  {
    // 15 color mode (low res)
    for (i = 0; i < 32; i++) 
    {
      if ((pictport & 0x20) && i == 16)
        addr += 512-16;

      pixel_t c = tftColor(Mem[addr+i] & 0x0F);
      for (j = scaleX(i*npix); j < scaleX(i*npix+1); j++)
      {
        pixmap[pmapindex++] = c;
      }

      c = tftColor(Mem[addr+i] >> 4);
      for (;j < scaleX(i*npix+2); j++)
      {
        pixmap[pmapindex++] = c;
      }
    }
  }
  else
  {
    // Two color mode (4x res mode)
    uint8_t odd = y & 0x01;

    for (i = 0; i < 32; i++)
    {
      if ((pictport & 0x20) && i == 16)
        addr += 512-16;

      pixel_t c = (Mem[addr+i] & (0x01 << 2*odd)) ? pictcolor : 0;
      for (j = scaleX(i*npix); j < scaleX(i*npix+1); j++)
      {
        pixmap[pmapindex++] = c;
      }

      c = (Mem[addr+i] & (0x02 << 2*odd)) ? pictcolor : 0;
      for (;j < scaleX(i*npix+2); j++)
      {
        pixmap[pmapindex++] = c;
      }

      c = (Mem[addr+i] & (0x10 << 2*odd)) ? pictcolor : 0;
      for (;j < scaleX(i*npix+3); j++)
      {
        pixmap[pmapindex++] = c;
      }

      c = (Mem[addr+i] & (0x20 << 2*odd)) ? pictcolor : 0;
      for (;j < scaleX(i*npix+4); j++)
      {
        pixmap[pmapindex++] = c;
      }
    }
  }

  for (i = scaleY(y); i < scaleY(y+1); i++)
  {
    tft.drawRGBBitmap(scaleX(0), i, pixmap, pmapindex, 1);
    #if DEBUGLVL>3
    printf("tft.drawRGBBitmap(%u, %u, pixmap, %u, 1)\r\n", scaleX(x), i+scaleY(y+n), pmapindex);
    #endif
  }
}

static void dazzler_lcd_full_redraw()
{
  #if DEBUGLVL>1
  printf("dazzler_lcd_full_draw()\n");
  #endif

  if (!(ctrlport & 0x80))
    return;
      
  int addr = (ctrlport & 0x7f) << 9;
  uint8_t x, y;

  pixel_t *pixmap = (uint16_t *) malloc(lcdWidth*sizeof(pixel_t));
  if (pixmap == NULL)
    return;

  for (y = 0; y < dazzres; y++, addr += 16) 
  {
    if (pictport & 0x20)
    {
      if (y == dazzres/2)
        addr += 512;
    }

    dazzler_lcd_draw_row(y, addr, pixmap);
    if (pictport & 0x40) 
    {
      y++;
      dazzler_lcd_draw_row(y, addr, pixmap);
    }
  }
  free(pixmap);
}
#endif // USELOWLEVELSPI
#endif // DAZZLCD

static void dazzler_send(const byte *data, uint16_t size)
{
  if( dazzler_iface<0xff )
    {
      size_t n, ptr = 0;
      while( size>0 )
        {
          n = host_serial_write(dazzler_iface, ((const char *) data)+ptr, size);
          ptr  += n;
          size -= n;
        }
    }
}



static void dazzler_send_fullframe(uint8_t buffer_flag, uint16_t addr)
{
  // send full picture memory
  byte b = DAZ_FULLFRAME | buffer_flag | (dazzler_mem_size > 512 ? 1 : 0);
  dazzler_send(&b, 1);
  dazzler_send(Mem+addr, dazzler_mem_size > 512 ? 2048 : 512);
}


static void dazzler_send_frame(int buffer_flag, uint16_t addr_old, uint16_t addr_new)
{
  uint8_t b[3];
  int n = 0, d = addr_new-addr_old, end = addr_new + dazzler_mem_size;

#if DEBUGLVL>2
  printf("dazzler_send_frame(%i, %04X, %04X)\n", buffer_flag, addr_old, addr_new);
#endif

  // check how many bytes actually differ between the 
  // old and new memory locations (if a program is
  // switching quickly between two locations then likely
  // not a large number of pixels/bytes will differ)
  for(int i=addr_new; i<=end; i++)
    if( Mem[i] != Mem[i-d] )
      n++;

  if( n*3 < dazzler_mem_size )
    {
      // it takes less data to send a diff than the full frame
      for(int i=addr_new; i<end; i++)
        if( Mem[i] != Mem[i-d] )
          {
#if DAZZLCD>0
            dazzler_lcd_draw_byte(i, Mem[i]);
#endif

            b[0] = DAZ_MEMBYTE | buffer_flag | (((i-addr_new) & 0x0700)/256);
            b[1] = i & 255;
            b[2] = Mem[i];
            dazzler_send(b, 3);
          }
    }
    else
    {
      // sending full frame is shorter
#if DAZZLCD>0
      dazzler_lcd_full_redraw();
#endif
      dazzler_send_fullframe(buffer_flag, addr_new);
    }
}


static void dazzler_check_version()
{
  if( dazzler_client_version<0 )
    {
      // client version not yet determined => send host version to client
      byte b = DAZ_VERSION | (DAZZLER_COMPUTER_VERSION&0x0F);
      dazzler_send(&b, 1);

      // wait to receive version response from client
      // client response is received via interrupt in dazzler_receive()
      unsigned long t = millis();
      while( millis()-t < 50 && dazzler_client_version<0 ) { host_check_interrupts(); TIMER_ADD_CYCLES(2); }

      // if client does not respond within 10ms then it is either not present
      // or version 0 (which did not understand DAZ_VERSION and ignored it)
      if( dazzler_client_version<0 ) 
        {
          dazzler_client_version=0;

          // both version 0 clients (PIC32/Windows) had these features
          dazzler_client_features = FEAT_VIDEO | FEAT_JOYSTICK | FEAT_FRAMEBUF;
        }

#if DEBUGLVL>0
      Serial.print(F("client is version ")); Serial.print(dazzler_client_version);
      Serial.print(F(" with features 0x")); Serial.println(dazzler_client_features, HEX);
#endif
    }
}

void dazzler_write_mem_do(uint16_t a, byte v)
{
  byte b[3];

#if DEBUGLVL>2
  printf("dazzler_write_mem(%04x, %02x)\n", a, v);
#endif

#if DAZZLCD>0
  dazzler_lcd_draw_byte(a, v);
#endif

  // buffer 1 must be in use if we get here
  if( ((uint16_t) (a-dazzler_mem_addr1))<dazzler_mem_size )
    {
      b[0] = DAZ_MEMBYTE | BUFFER1 | (((a-dazzler_mem_addr1) & 0x0700)/256);
      b[1] = a & 0xFF;
      b[2] = v;
      dazzler_send(b, 3);
    }

  // buffer 2 may or may not be in use
  if( dazzler_mem_addr2!=0xFFFF && ((uint16_t) (a-dazzler_mem_addr2))<dazzler_mem_size )
    {
      b[0] = DAZ_MEMBYTE | BUFFER2 | (((a-dazzler_mem_addr2) & 0x0700)/256);
      b[1] = a & 0xFF;
      b[2] = v;
      dazzler_send(b, 3);
    }
}


void dazzler_out_ctrl(byte v)
{
  byte b[3];
  
  ctrlport = v;
  
  b[0] = DAZ_CTRL;

#if DEBUGLVL>0
  {
    static byte prev = 0xff;
    if( v!=prev ) { printf("dazzler_out_ctrl(%02x)\n", v); prev = v; }
  }
#endif

  dazzler_check_version();

  // if client uses a framebuffer then we need to send CTRL before FULLFRAME data
  // so the client renders the data with the new properties
  if( dazzler_client_features & FEAT_FRAMEBUF ) { b[1] = v; dazzler_send(b, 2); }

  // D7: 1=enabled, 0=disabled
  // D6-D0: bits 15-9 of dazzler memory address
  uint16_t a = (v & 0x7f) << 9;
  if( (v & 0x80)==0 )
    {
      // dazzler is turned off
      dazzler_mem_addr1 = 0xFFFF;
      dazzler_mem_addr2 = 0xFFFF;
#if DAZZLCD>0
      tft.fillRect(lcdOffsetX, lcdOffsetY, lcdWidth, lcdWidth, ILI9341_DARKCYAN);
#endif
      v = 0x00;
    }
  else if( (dazzler_client_features & FEAT_DUAL_BUF)==0 )
    {
      // client only has a single buffer
      if( dazzler_mem_addr1==0xFFFF )
        {
          // not yet initialized => send full frame
#if DAZZLCD>0
          dazzler_lcd_full_redraw();
#endif
          dazzler_send_fullframe(BUFFER1, a);
          dazzler_mem_addr1 = a;
        }
      else if( a!=dazzler_mem_addr1 )
        {
          // already initialized and different address => send diff
          dazzler_send_frame(BUFFER1, dazzler_mem_addr1, a);
          dazzler_mem_addr1 = a;
        }
    }
  else if( a==dazzler_mem_addr1 )
    {
      // buffer for this address range is already initialized
      v = 0x80;
    }
  else if( a==dazzler_mem_addr2 )
    {
      // buffer for this address range is already initialized
      v = 0x81;
    }
  else if( dazzler_mem_addr1==0xFFFF )
    {
      // new address range, buffer 1 is available => use it
      dazzler_mem_addr1 = a;
      dazzler_send_fullframe(BUFFER1, dazzler_mem_addr1);
      v = 0x80;
    }
  else if( dazzler_mem_addr2==0xFFFF )
    {
      // new address range, buffer 2 is available => use it
      dazzler_mem_addr2 = a;
      dazzler_send_fullframe(BUFFER2, dazzler_mem_addr2);
      v = 0x81;
    }
  else 
    {
      // new address range, both buffers are in use => pick one to overwrite
      static bool first = true;
      if( first )
        {
          dazzler_send_frame(BUFFER1, dazzler_mem_addr1, a);
          dazzler_mem_addr1 = a;
          v = 0x80;
        }
      else
        {
          dazzler_send_frame(BUFFER2, dazzler_mem_addr2, a);
          dazzler_mem_addr2 = a;
          v = 0x81;
        }

      first = !first;
    }

  // determine rough address range (for memory writes)
  if( dazzler_mem_addr1==0xFFFF )
    {
      // no buffer occupied
      dazzler_mem_start = 0x0000;
      dazzler_mem_end   = 0x0000;
    }
  else if( dazzler_mem_addr2==0xFFFF )
    {
      // only buffer 1 occupied
      dazzler_mem_start = dazzler_mem_addr1;
      dazzler_mem_end   = dazzler_mem_start + dazzler_mem_size;
    }
  else
    {
      // both buffers occupied
      dazzler_mem_start = min(dazzler_mem_addr1, dazzler_mem_addr2);
      dazzler_mem_end   = max(dazzler_mem_addr1, dazzler_mem_addr2) + dazzler_mem_size;
    }

  // if client does not have a framebuffer (i.e. renders in real-time) then we can
  // send CTRL after FULLFRAME data (avoids initial short initial incorrect display)
  if( !(dazzler_client_features & FEAT_FRAMEBUF) ) { b[1] = v; dazzler_send(b, 2); }


}


void dazzler_out_pict(byte v)
{
  // D7: not used
  // D6: 1=resolution x4 (single color), 0=normal resolution (multi-color)
  // D5: 1=2k memory, 0=512byte memory
  // D4: 1=color, 0=monochrome
  // D3-D0: color info for x4 high res mode


#if DAZZLCD>0
  if (v != pictport) {
    dazzres = ((v & 0x20) ? 64 : 32) * ((v & 0x40) ? 2 : 1);
    //if ((v&0x40) || (v&0x70 != pictport&0x70))
    {
      pictport = v;
      pictcolor = tftColor(v & 0x0F);
      dazzler_lcd_full_redraw();
    }
#if DEBUGLVL>0
    printf("pictport = %02x, pictcolor = (%02x)\n", pictport, pictcolor);
#endif
  }
#endif

pictport = v;

#if DEBUGLVL>0
  static byte prev = 0xff;
  if( v!=prev ) { printf("dazzler_out_pict(%02x)\n", v); prev = v; }
#endif

  dazzler_check_version();

  byte b[2];
  b[0] = DAZ_CTRLPIC;
  b[1] = v;

  // if client uses a framebuffer then we need to send  CTRLPIC before FULLFRAME data
  // so the client renders the data with the new properties
  if( dazzler_client_features & FEAT_FRAMEBUF ) { b[1] = v; dazzler_send(b, 2); }

  uint16_t s = v & 0x20 ? 2048 : 512;
  if( s > dazzler_mem_size )
    {
      if( dazzler_mem_end>0 ) dazzler_mem_end += s-dazzler_mem_size;

      // switching to a bigger memory size => send memory again
      dazzler_mem_size = s;
      if( dazzler_mem_addr1!=0xFFFF )
        dazzler_send_fullframe(BUFFER1, dazzler_mem_addr1);
      if( dazzler_mem_addr2!=0xFFFF )
        dazzler_send_fullframe(BUFFER2, dazzler_mem_addr2);
    }
  else
    dazzler_mem_size = s;

  // if client does not have a framebuffer (i.e. renders in real-time) then we can
  // send CTRL after FULLFRAME data (avoids initial short initial incorrect display)
  if( !(dazzler_client_features & FEAT_FRAMEBUF) ) { b[1] = v; dazzler_send(b, 2); }

}


void dazzler_out_dac(byte dacnum, byte v)
{
  static byte     prev_sample[7] = {0, 0, 0, 0, 0, 0, 0};
  static uint32_t prev_sample_cycles[7] = {0, 0, 0, 0, 0, 0, 0};

  if( (dazzler_client_features & FEAT_DAC) && v!=prev_sample[dacnum] )
    {
      uint32_t c = timer_get_cycles();
      uint16_t delay = min(65535, (c - prev_sample_cycles[dacnum])/2);

      byte b[4];
      b[0] = DAZ_DAC | (dacnum & 0x0F);
      b[1] = delay & 255;
      b[2] = delay / 256;
      b[3] = v;
      dazzler_send(b, 4);

      prev_sample[dacnum] = v;
      prev_sample_cycles[dacnum] = c;
    }

  TIMER_ADD_CYCLES(11);
}


inline void set_d7a_port(byte p, byte v)
{
#if DEBUGLVL>1
  printf("set_d7a_port(%02x, %02x)\n", 0030+p, v);
#endif
  d7a_port[p]=v;
}


void dazzler_receive(byte iface, byte data)
{
  static byte state=0, bufdata[3];

#if DEBUGLVL>1
  Serial.print("dazzler_receive: "); Serial.println(data, HEX);
#endif

  switch( state )
    {
    case 0:
      {
        state = data & 0xf0;
        switch( state )
          {
          case DAZ_JOY1:
            set_d7a_port(0, (d7a_port[0] & 0xF0) | (data & 0x0F));
            break;
            
          case DAZ_JOY2:
            set_d7a_port(0, (d7a_port[0] & 0x0F) | ((data & 0x0F)*16));
            break;

          case DAZ_KEY:
            break;

          case DAZ_VERSION:
            {
              byte version = data & 0x0F;
              if( version<2 )
                {
                  // client version 0 did not send DAZ_VERSION, client version 1 (only PIC32) had these features:
                  dazzler_client_features = FEAT_VIDEO | FEAT_JOYSTICK | FEAT_DUAL_BUF | FEAT_VSYNC;
                  dazzler_client_version = version; 
                  state = 0;
                }
              else
                {
                  // version 2 and up report features => wait for 2 more bytes of data
                  bufdata[0] = version;
                }
              break;
            }

          case DAZ_VSYNC:
            dazzler_vsync_cycles = timer_get_cycles();
            state = 0;
            break;

          default:
            state = 0;
          }
        break;
      }

    case DAZ_JOY1:
      set_d7a_port(1, data);
      state++;
      break;

    case DAZ_JOY1+1:
      set_d7a_port(2, data);
      state = 0;
      break;

    case DAZ_JOY2:
      set_d7a_port(3, data);
      state++;
      break;

    case DAZ_JOY2+1:
      set_d7a_port(4, data);
      state = 0;
      break;

    case DAZ_KEY:
      {
        int i = config_serial_map_sim_to_host(CSM_SIO);
        if( i<0xff ) serial_receive_host_data(i, data);
        state = 0;
        break;
      }

    case DAZ_VERSION:
      bufdata[1] = data;
      state++;
      break;

    case DAZ_VERSION+1:
      dazzler_client_features = bufdata[1] + data * 256;
      dazzler_client_version  = bufdata[0];
      state = 0;
      break;
    }
}


byte dazzler_in(byte port)
{
  byte v = 0;

  if( port==0016 )
    {
      // the values here are approximated and certainly not
      // synchronized with the actual picture on the client
      // we just provide them here so programs waiting for the
      // signals don't get stuck.
      // If I understand correctly, the Dazzler does not
      // interlace two fields and instead counts each field
      // (half-frame) as a full frame (of 262 lines). Therefore 
      // its frame rate is 29.97 * 2 = 59.94Hz, i.e. 16683us per frame,
      // i.e. 33367 cycles per frame.
      const uint32_t cycles_per_frame = 33367;
      const uint32_t cycles_per_line  = cycles_per_frame/262;

      uint32_t c, cc = timer_get_cycles();

      // determine position within frame
      // (if client does not send VSYNC then compute an aproximation)
      if( dazzler_client_features & FEAT_VSYNC )
        c = cc - dazzler_vsync_cycles;
      else
        c = cc % cycles_per_frame;

      // bits 0-5 are unused
      v = 0xff;

      // bit 6: is low for 4ms (8000 cycles) between frames
      // (we pull it low at the beginning of a frame)
      // note that if bit 6 is low then bit 7 is also low,
      // according to the Dazzler schematics: The 7493 counter
      // which produces the bit 7 output is continuously reset
      // while bit 6 is low.
      if( c<8000 ) v &= ~0xC0;

      // bit 7: low for odd line, high for even line
      if( (c/cycles_per_line)&1 ) v &= ~0x80;
    }
  else if( port>=0030 && port<0035 )
    {
      // D+7A I/O board
      // The D+7A board was not part of the Dazzler but we include
      // it in the Dazzler emulation to support joysticks
      v = d7a_port[port-0030];
    }
  
#if DEBUGLVL>2
  printf("%04x: dazzler_in(%i)=%02x\n", regPC, port, v);
#endif

  return v;
}


void dazzler_set_iface(byte iface)
{
  static host_serial_receive_callback_tp fprev = NULL;

  if( iface != dazzler_iface )
    {
      // if we had an interface set, restore that interface's previous receive callback
      if( dazzler_iface<0xff ) host_serial_set_receive_callback(dazzler_iface, fprev);
      
      // set receive callback
      dazzler_iface = iface;
      dazzler_client_version = -1;
      dazzler_client_features = 0;
      fprev = host_serial_set_receive_callback(dazzler_iface, dazzler_receive);
      
#if DEBUGLVL>0
      if( iface==0xff ) Serial.println("Dazzler disabled"); else {Serial.print("Dazzler on interface:"); Serial.println(iface);}
#endif
    }
}


byte dazzler_get_iface()
{
  return dazzler_iface;
}


void dazzler_setup()
{
  dazzler_mem_size  = 512;
  dazzler_mem_addr1 = 0xFFFF;
  dazzler_mem_addr2 = 0xFFFF;
  dazzler_mem_start = 0x0000;
  dazzler_mem_end   = 0x0000;
  dazzler_client_version = -1;
  dazzler_client_features = 0;

  dazzler_set_iface(config_dazzler_interface());

#if DAZZLCD>0
  SPIGuard spi;

  tft.begin();

#if ILI9341> 0
  // read diagnostics (optional but can help debug problems)
  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX); 

  tft.fillScreen(ILI9341_BLACK);
#endif

  tft.setRotation(TFT_ROTATION);

#endif
}

#endif
