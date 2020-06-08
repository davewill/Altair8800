#ifdef __SAM3X8E__

#include "config.h"

#if DAZZLCD>0

#include "dazzler.h"
#include "mem.h"
#include "cpucore.h"
#include "host.h"
#include "serial.h"
#include "timer.h"
#include "numsys.h"
#include <malloc.h>


#include "SPI.h"
#include "Adafruit_GFX.h"

volatile byte ctrlport = 0x00;
volatile byte pictport = 0x00;

uint8_t currlayer = BUFFER1;
boolean layer1dirty = false, layer2dirty = false;


#if ILI9341>0
#include "Adafruit_ILI9341.h"

#define TFT_DC   9 // This display needs an extra IO pin for command/data
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

static uint16_t
  pixelWidth  = ILI9341_TFTWIDTH,  // TFT dimensions
  pixelHeight = ILI9341_TFTHEIGHT;
typedef uint16_t pixel_t;
volatile pixel_t pictcolor = 0xffff;

#define TFT_ROTATION 3
#define TOPBAR 0
#define SIDEBAR 0

#if (TFT_ROTATION & 0x01) == 1
static uint16_t
  dazzOffsetX = 0,
  dazzOffsetY = 0,
  dazzHeight = pixelWidth,
  dazzWidth = min(pixelWidth*4/3, pixelHeight-SIDEBAR);
  #else
static uint16_t
  dazzOffsetX = 0,
  dazzOffsetY = TOPBAR,
  dazzHeight = min(pixelWidth, pixelHeight-TOPBAR),
  dazzWidth = pixelWidth;
#endif

#define Color565  color565
#elif USERA8875>0
#include <RA8875.h>

#define DEBUGLVL 0

RA8875 tft = RA8875(TFT_CS);

static uint16_t
  pixelWidth  = 800,  // TFT dimensions
  pixelHeight = 480;
typedef uint16_t pixel_t;
volatile pixel_t pictcolor = RA8875_MAGENTA;

#define TFT_ROTATION 0
#define TOPBAR 0
#define SIDEBAR 0

static uint16_t
  termOffsetX, termOffsetY, termHeight, termWidth,
  dazzOffsetX, dazzOffsetY, dazzHeight, dazzWidth;
#endif

boolean dazzEnabled = false;
boolean termEnabled = false;

uint8_t dazzres, screenconfig;
#define scaleX(dazzX) (dazzOffsetX+((dazzX)*dazzWidth/dazzres)) // ((256/dazzres)*(dazzX)*240/256)
#define scaleY(dazzY) (dazzOffsetY+((dazzY)*dazzHeight/dazzres)) // ((256/dazzres)*(dazzY)*240/256)

void lcd_set_screen_config(enum lcdLayout config)
{

#if USERA8875>0
  switch (config)
  {
    case LAYOUT_DAZZ:
      termEnabled = false;
      dazzEnabled = true;
      dazzOffsetX = 0;
      dazzOffsetY = 0;
      dazzHeight = pixelHeight;
      dazzWidth = min(pixelHeight*4/3, pixelWidth-SIDEBAR);
      break;

   #if TERMLCD>0
   case LAYOUT_TERM:
      termEnabled = true;
      dazzEnabled = false;
      termOffsetX = 0;
      termOffsetY = 0;
      termHeight = pixelHeight;
      termWidth = min(pixelHeight*4/3, pixelWidth-SIDEBAR);
      break;

    case LAYOUT_DAZZ_MAIN:
      dazzEnabled = true;
      dazzWidth = pixelWidth/2;
      dazzHeight = dazzWidth*3/4;
      dazzOffsetX = pixelWidth/2;
      dazzOffsetY = pixelHeight-dazzHeight;

      termEnabled = true;
      termWidth = pixelWidth/2;
      termHeight = pixelHeight;
      termOffsetX = 0;
      termOffsetY = 0;
      break;

    case LAYOUT_TERM_MAIN:
      dazzEnabled = true;
      dazzWidth = pixelWidth/2;
      dazzHeight = dazzWidth*3/4;
      dazzOffsetX = 0;
      dazzOffsetY = pixelHeight-dazzHeight;

      termEnabled = true;
      termWidth = pixelWidth/2;
      termHeight = pixelHeight;
      termOffsetX = pixelWidth/2;
      termOffsetY = 0;
      break;
  #endif

    default:
      // bad config
      return;
  }

  screenconfig = config;

  {
    SPIGuard spi;
    tft.clearScreen();
    lcd_term_full_redraw();
    dazzler_lcd_full_redraw(BUFFER1, dazzler_mem_addr1);
  }
#endif

}

void lcd_term_write(const char *buf, size_t n)
{
  #if TERMLCD>0
  if (termEnabled) 
  {
    SPIGuard spi;

    #if 0
    tft.setScrollWindow();
    for (int i = 0; i < n; i++)
    {
    }
    #else
    tft.setActiveWindow(termOffsetX, termOffsetX+termWidth-1, termOffsetY, termOffsetY+termHeight-1);
    tft.write(buf, n);
    tft.setActiveWindow();
    #endif
  }
  #endif
}

void lcd_term_full_redraw()
{
  #if TERMLCD>0

  if (termEnabled) 
  {
    SPIGuard spi;

    tft.setActiveWindow(termOffsetX, termOffsetX+termWidth-1, termOffsetY, termOffsetY+termHeight-1);
    tft.setCursor(termOffsetX,termOffsetY);
    tft.write("Once upon a midnight dreary, while I pondered, weak and weary,\r\n", 0);
    tft.write("Over many a quaint and curious volume of forgotten lore,\r\n", 0);
    tft.write("While I nodded, nearly napping, suddenly there came a tapping,\r\n", 0);
    tft.write("As of some one gently rapping, rapping at my chamber door.\r\n", 0);
    tft.write("'Tis some visitor,' I muttered, 'tapping at my chamber door Only this, and nothing more.'\r\n", 0);
    tft.write("\r\n", 0);
    tft.write("Ah, distinctly I remember it was in the bleak December,\r\n", 0);
    tft.write("And each separate dying ember wrought its ghost upon the floor.\r\n", 0);
    tft.write("Eagerly I wished the morrow;- vainly I had sought to borrow\r\n", 0);
    tft.write("From my books surcease of sorrow- sorrow for the lost Lenore-\r\n", 0);
    tft.write("For the rare and radiant maiden whom the angels name Lenore-\r\n", 0);
    tft.write("Nameless here for evermore.\r\n", 0);
    tft.write("");
    tft.setActiveWindow();
  }
  #endif
}

static void setLayertoWrite(RA8875writes d)
{
  static RA8875writes last = L1;

  if (last != d)
  {
    last = d;
    tft.writeTo(d);
  }
}

static pixel_t tftColor(uint8_t v)
{
  if (pictport & 0x10)
  {    
  uint8_t i = v & 0x08;
  return tft.Color565(((v & 0x01) ? 0xff : 0) / (i ? 1 : 3),
                      ((v & 0x02) ? 0xff : 0) / (i ? 1 : 3),
                      ((v & 0x04) ? 0xff : 0) / (i ? 1 : 3));
  }  
  else
  {
    uint8_t gray = ((v&0x0f) << 4) | ((v&0x0f));
    return tft.Color565(gray, gray, gray);
  }
}

void getXY(uint16_t index, uint8_t &x, uint8_t &y)
{
  uint8_t npix = (pictport & 0x40) ? 4 : 2;
  
  if (!(pictport & 0x20))
  {
    x = npix*(index % (dazzres/npix));
    y = (index / (dazzres/npix));

    // Each byte is two lines tall
    if (pictport & 0x40)  y *= 2;
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
    pixel_t lc = tftColor(v&0x0F), 
            rc = tftColor(v>>4);
    if (lc == rc)
    {
      tft.fillRect(scaleX(x), scaleY(y), scaleX(x+2)-scaleX(x), scaleY(y+1)-scaleY(y), lc);
    }
    else
    {
      tft.fillRect(scaleX(x), scaleY(y), scaleX(x+1)-scaleX(x), scaleY(y+1)-scaleY(y), lc);
      tft.fillRect(scaleX(x+1), scaleY(y), scaleX(x+2)-scaleX(x+1), scaleY(y+1)-scaleY(y), rc);
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

void dazzler_lcd_update_pictport(uint8_t v)
{
  // D7: not used
  // D6: 1=resolution x4 (single color), 0=normal resolution (multi-color)
  // D5: 1=2k memory, 0=512byte memory
  // D4: 1=color, 0=monochrome
  // D3-D0: color info for x4 high res mode

  if (v != pictport) {
    dazzres = ((v & 0x20) ? 64 : 32) * ((v & 0x40) ? 2 : 1);
    // if ((v&0x40) || (v&0x70 != pictport&0x70))
    {
      SPIGuard spi;
      uint8_t old_pp = pictport;
      pictport = v;
      
        pictcolor = RA8875_MAGENTA; //tftColor(v & 0x0F);
      if (v&0x40)
      {
        tft.setTransparentColor(RA8875_MAGENTA);
        setLayertoWrite(L2);
        tft.fillRect(dazzOffsetX, dazzOffsetY, dazzWidth, dazzHeight, tftColor(v & 0x0F));
        tft.layerEffect(TRANSPARENT);
      }
      else
      {
        tft.layerEffect(LAYER1);
        // pictcolor = tftColor(v & 0x0F);
      }
      
      
      if ((v&0xF0) != (old_pp&0xF0))
      {
        pictport = v;
        if (currlayer == BUFFER1)
        {
          dazzler_lcd_full_redraw(BUFFER1, dazzler_mem_addr1, 1);
          layer2dirty = true;
        }
        else
        {
          dazzler_lcd_full_redraw(BUFFER2, dazzler_mem_addr2, 1);
          layer1dirty = true;
        }
      }


    }
#if DEBUGLVL>0
    printf("pictport = %02x\n", pictport);
#endif
  }

}

void dazzler_lcd_update_ctrlport(uint8_t v)
{
  ctrlport = v;
}

void dazzler_lcd_clear(void)
{
  SPIGuard spi;

  if (dazzEnabled)
    tft.fillRect(dazzOffsetX, dazzOffsetY, dazzWidth, dazzHeight, 0);
}

void dazzler_lcd_draw_byte(uint8_t buffer_flag, uint16_t a, byte v)
{
    uint16_t baseaddr = buffer_flag == BUFFER1 ? dazzler_mem_addr1 : dazzler_mem_addr2;
    int32_t index = (int32_t) a-baseaddr;
    uint8_t x, y;

  if (!dazzEnabled || !(ctrlport & 0x80))
  {
    return;
  }
      
#if DEBUGLVL>2
  if (faddr < 5)
    printf("dazzler_lcd_draw_byte(%u, %u), faddr = %u\n", a, v, faddr);
#endif

    if ((ctrlport & 0x80) && index >= 0 && index < ((pictport & 0x20) ? 2048 : 512))
    {

      if (buffer_flag == BUFFER1 && layer1dirty)
      {
        dazzler_lcd_full_redraw(BUFFER1, dazzler_mem_addr1);
        layer1dirty = false;
      }
      else if (buffer_flag == BUFFER2 && layer2dirty)
      {
        dazzler_lcd_full_redraw(BUFFER2, dazzler_mem_addr1);
        layer2dirty = false;
      }
      else
      {
        if (buffer_flag == BUFFER1)
          setLayertoWrite(L1);
        else
          setLayertoWrite(L2);

        getXY(index, x, y);

        dazzler_lcd_draw_byte_xy(x, y, v);
      }
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

void dazzler_lcd_full_redraw(boolean colorchangeonly)
{
  #if DEBUGLVL>1
  printf("dazzler_lcd_full_draw()\n");
  #endif

  if (!dazzEnabled || !(ctrlport & 0x80))
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

  pixel_t *pixmap = (uint16_t *) malloc(dazzWidth*sizeof(pixel_t));
  if (pixmap == NULL)
    return;

  tft.startWrite();
  tft.setAddrWindow(dazzOffsetX, dazzOffsetY, dazzWidth, dazzHeight); // Clipped area
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
#elif 1
static void dazzler_lcd_draw_row(uint8_t y, uint8_t buffer_flag, uint16_t addr, pixel_t *pixmap)
{
  uint8_t npix = (pictport & 0x40) ? 4 : 2;
  uint16_t i, j, pmapindex = 0;
  SPIGuard spi;

  #if DEBUGLVL>2
  printf("dazzler_lcd_draw_row(%u, %u, %04x)\r\n", x, y, addr);
  #endif

  if (buffer_flag == BUFFER1)
    setLayertoWrite(L1);
  else
    setLayertoWrite(L2);

  if (!(pictport & 0x40))
  {
    // 15 color mode (low res)
    for (i = 0; i < dazzres/2; i++) 
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

    for (i = 0; i < dazzres/4; i++)
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
    #if ILI9341>0
    tft.drawRGBBitmap(scaleX(0), i, pixmap, pmapindex, 1);
    #elif USERA8875>0
    tft.drawPixels(pixmap, pmapindex, scaleX(0), i);
    #endif
    #if DEBUGLVL>3
    printf("tft.drawRGBBitmap(%u, %u, pixmap, %u, 1)\r\n", scaleX(x), i+scaleY(y+n), pmapindex);
    #endif
  }
}

void dazzler_lcd_full_redraw(uint8_t buffer_flag, uint16_t addr, boolean colorchangeonly)
{
  #if DEBUGLVL>0
  printf("full_draw()\n");
  #endif

  if (!dazzEnabled || !(ctrlport & 0x80))
    return;
      
  //int addr = (ctrlport & 0x7f) << 9;
  uint8_t x, y;

  pixel_t *pixmap = (uint16_t *) malloc(dazzWidth*sizeof(pixel_t));
  if (pixmap == NULL)
    return;

  for (y = 0; y < dazzres; y++, addr += 16) 
  {
    if (pictport & 0x20)
    {
      if (y == dazzres/2)
        addr += 512;
    }

    dazzler_lcd_draw_row(y, buffer_flag, addr, pixmap);
    if (pictport & 0x40) 
    {
      y++;
      dazzler_lcd_draw_row(y, buffer_flag, addr, pixmap);
    }
  }
  free(pixmap);
}
#else
void dazzler_lcd_full_redraw(boolean colorchangeonly)
{
#if DEBUGLVL>1
  printf("dazzler_lcd_full_draw(%04X)\n", addr);
#endif

  if (!dazzEnabled || !(ctrlport & 0x80))
    return;
      
  uint8_t res = ((pictport & 0x20) ? 64 : 32) * ((pictport & 0x40) ? 2 : 1);
  uint8_t npix = (pictport & 0x40) ? 4 : 2;
  int addr = (ctrlport & 0x7f) << 9;
  uint8_t x, y;

  if ((pictport & 0x20))
  {
    for (y = 0; y < res/2; y++) 
    {
      for (x = 0; x < res/2; x+=npix, addr++) 
      {
        uint8_t v = *(Mem+addr);
        dazzler_lcd_draw_byte_xy(x, y, v, colorchangeonly);
        v = *(Mem+addr+512);
        dazzler_lcd_draw_byte_xy(x+res/2, y, v, colorchangeonly);
        v = *(Mem+addr+1024);
        dazzler_lcd_draw_byte_xy(x, y+res/2, v, colorchangeonly);
        v = *(Mem+addr+1536);
        dazzler_lcd_draw_byte_xy(x+res/2, y+res/2, v, colorchangeonly);
      }
      if (pictport & 0x40)
        y++;
    }
  }
  else
  {
    for (y = 0; y < res; y++) 
    {
      for (x = 0; x < res; x+=npix, addr++) 
      {
        uint8_t v = *(Mem+addr);
        dazzler_lcd_draw_byte_xy(x, y, v, colorchangeonly);
      }
      if (pictport & 0x40)
        y++;
    }
  }
}
#endif // USELOWLEVELSPI

#if USETOUCH>0
void lcdCheckTouch(void)
{
  uint16_t x, y;
  static uint32_t last = 0;
  uint32_t now = millis();

  SPIGuard spi;

  if (now-last > 100 && tft.touched(true))
  {
    tft.touchReadPixel(&x, &y);
    if (millis() - last > 200)
    {
      Serial.print("Touch(");
      Serial.print(x);
      Serial.print(", ");
      Serial.print(y);
      Serial.println(")");
      lcd_set_screen_config((enum lcdLayout) ((screenconfig+1) % NUMLAYOUTS));
    }
    last = millis();
  }
  else
  {
    if (millis() - last < 200)
      Serial.print(".");
  }
}
#endif

uint8_t lcd_read_controllers(uint8_t chan)
{
    static uint32_t lastread[] = { 0, 0, 0, 0};
    static uint32_t lasttime[] = { 0, 0, 0, 0};

    if (millis() - lasttime[chan] > 16)
    {
      SPIGuard spi;
      tft.WriteRegister(RA8875_GPO, ~(1 << chan));
      lastread[chan] = ~tft.ReadRegister(RA8875_GPI);
      lasttime[chan] = millis();
    }

    return lastread[chan];
}

byte lcd_read_joy(uint8_t port)
{
  uint8_t v1, v2;
  uint8_t rtn = 0;

  switch (port)
  {
    case 0: // Buttons from both lcd_read_controllers
      v1 = lcd_read_controllers(0);
      v2 = lcd_read_controllers(2);
      rtn = ~((v1 & 0x0f) | (v2 << 4));
      break;

    case 2: // controller 1 X-Axis
      v1 = lcd_read_controllers(1);
      if (v1 & 0x01)
        rtn = 126;
      else if (v1 & 0x2)
        rtn = -127;
      break;

    case 1: // controller 1 Y-Axis
      v1 = lcd_read_controllers(1);
      if (v1 & 0x08)
        rtn = 126;
      else if (v1 & 0x4)
        rtn = -127;
      break;

    case 4: // controller 2 X-Axis
      v1 = lcd_read_controllers(3);
      if (v1 & 0x01)
        rtn = 126;
      else if (v1 & 0x2)
        rtn = -127;
      break;

    case 3: // controller 2 Y-Axis
      v1 = lcd_read_controllers(3);
      if (v1 & 0x08)
        rtn = 126;
      else if (v1 & 0x4)
        rtn = -127;
      break;
  }

  return rtn;
}

void dazzler_lcd_setlayer(uint8_t buffer_flag)
{
  #if DEBUGLVL>0
  printf("setlayer(%d)\n", buffer_flag);
  #endif
  if (buffer_flag == BUFFER1 && layer1dirty)
  {
    dazzler_lcd_full_redraw(BUFFER1, dazzler_mem_addr1);
    layer1dirty = false;
  }
  else if (buffer_flag == BUFFER2 && layer2dirty)
  {
    dazzler_lcd_full_redraw(BUFFER2, dazzler_mem_addr2);
    layer2dirty = false;
  }

  currlayer = buffer_flag;
  if (pictport & 0x40)
  {
    tft.layerEffect(TRANSPARENT);
  }
  else
  {
    tft.layerEffect(buffer_flag == BUFFER1 ? LAYER1 : LAYER2);
  }
}


void lcd_setup(void)
{
  SPIGuard spi;

#if ILI9341> 0
  tft.begin();

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
#elif USERA8875>0
  tft.begin(RA8875_800x480,8);
  tft.clearScreen();

  setLayertoWrite(L1);
#endif

#if USETOUCH>0
  tft.useINT(60);
  tft.touchBegin();
  tft.enableISR(true);
#endif

  tft.setRotation(TFT_ROTATION);

#if TERMLCD>0
  lcd_set_screen_config(LAYOUT_TERM_MAIN);
#else
  lcd_set_screen_config(LAYOUT_DAZZ);
#endif
}
#endif // DAZZLCD
#endif
