#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include <Wire.h>
#include "Sprites.h"

/*****************************************************************************/

constexpr int16_t LCD_X_OFFSET = 0;    // Panel-specific vertical offset.
constexpr int16_t LCD_Y_OFFSET = 20;   // Panel-specific vertical offset.
Arduino_ESP32SPI bus(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_ST7789 gfx(
    &bus,
    LCD_RST,
    0,            // rotation (90 degrees)
    true,         // IPS
    LCD_WIDTH,
    LCD_HEIGHT,
    LCD_X_OFFSET,
    LCD_Y_OFFSET,
    0, 0
);

/*****************************************************************************/

// Draw an RGB565 image centered on the screen (write* version).
// Assumes caller wraps with startWrite()/endWrite().
// img: pointer to w*h pixels, RGB565 packed in a uint8_t buffer
// w,h: image dimensions in pixels (must be <= screen size)
static inline void writeImageCentered(const uint8_t* img, int w, int h)
{
  const int dstX = (gfx.width() - w) / 2;
  const int dstY = (gfx.height() - h) / 2;

  gfx.writeAddrWindow(dstX, dstY, w, h);
  gfx.writePixels((uint16_t*)img, w * h);
}

// Tile an RGB565 image across the entire screen (write* version).
// Assumes caller wraps with startWrite()/endWrite().
// img: pointer to w*h pixels, RGB565 packed in a uint8_t buffer
// w,h: image dimensions in pixels (assumed > 0 and <= screen size)
static inline void writeImageTiled(const uint8_t* img, int w, int h)
{
  const int scrW = gfx.width();
  const int scrH = gfx.height();

  for (int y = 0; y < scrH; y += h)
  {
    const int drawH = (y + h <= scrH) ? h : (scrH - y);

    for (int x = 0; x < scrW; x += w)
    {
      const int drawW = (x + w <= scrW) ? w : (scrW - x);

      if (drawW == w && drawH == h)
      {
        // Fast path: whole tile fits – stream in one shot
        gfx.writeAddrWindow(x, y, w, h);
        gfx.writePixels((uint16_t*)img, w * h);
      }
      else
      {
        // Edge tiles: clip by streaming row-by-row from the source image
        for (int row = 0; row < drawH; ++row)
        {
          const uint8_t* srcLine = img + row * (w * 2); // 2 bytes per pixel
          gfx.writeAddrWindow(x, y + row, drawW, 1);
          gfx.writePixels((uint16_t*)srcLine, drawW);
        }
      }
    }
  }
}

// Stretch an RGB565 image vertically to a target width by duplicating
// the first source row downwards, and drawing the original image on the bottom.
// Assumes caller wraps with startWrite()/endWrite().
// img: pointer to w*h pixels, RGB565 packed in a uint8_t buffer
// w,h: source image dimensions
// x,y: top-left of the stretched bar area on screen
// targetH: total height of the stretched area in pixels
static inline void writeImageStretched(const uint8_t* img, int w, int h, int x, int y, int targetH)
{
  const int fillH = (targetH > h) ? (targetH - h) : 0;

  // 1) Fill the top area using the first row of the image
  if (fillH > 0)
  {
    for (int col = 0; col < w; ++col)
    {
      // First pixel of col (2 bytes per pixel)
      uint16_t p = *(uint16_t*)(img + col * 2);
      gfx.writeFastVLine(x + col, y, fillH, p);
    }
  }

  // 2) Draw the original image so its top aligns with y + fillH
  //    i.e. its left starts at x + leftFillW
  gfx.writeAddrWindow(x, y + fillH, w, h);
  gfx.writePixels((uint16_t*)img, w * h);
}

// Draw up to `height` pixels of the source image.
// Assumes caller wraps with startWrite()/endWrite().
// img: pointer to w*h pixels, RGB565 packed in a uint8_t buffer
// w,h: source image dimensions
// x,y: top-left target position on screen
// height: number of rows from the source image to draw (<= h)
static inline void writeImageClipped(const uint8_t* img, int w, int h, int x, int y, int height)
{
  if (height > h) height = h;

  gfx.writeAddrWindow(x, y, w, height);
  gfx.writePixels((uint16_t*)img, w * height);
}

/*****************************************************************************/

HWCDC USBSerial;

void setup(void) {
  USBSerial.begin(115200);
  USBSerial.println("Brickintosh Demo by DeanTheCoder");
  USBSerial.println("https://github.com/deanthecoder/Brickintosh");

  // Init Display
  if (!gfx.begin()) {
    USBSerial.println("gfx.begin() failed, halting.");
    while (1);
  }

  gfx.fillScreen(BLACK);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  delay(2000);
}

static void runSpeccyBoot() {
  gfx.fillScreen(0xd69a);
  delay(1000);

  // Black flash.
  const int w = 192;
  const int h = 256;
  const int x = (gfx.width() - w) / 2;
  const int y = (gfx.height() - h) / 2;
  gfx.fillRect(x, y, w, h, BLACK);
  delay(800);

  // Shrinking red bars.
  const float durationMs = 600.0f;
  const long startTime = millis();
  float p;
  while ((p = (millis() - startTime) / durationMs) <= 1.0f) {
    // Lines go from small to big to small again.
    p *= 2.0f;
    if (p > 1.0f)
      p = 2.0f - p;

    gfx.startWrite();
    for (int i = 8; i < h; i += 8) {
      int l = int(p * (w - 8));

      // Draw red.
      gfx.writeFastHLine(x + 4, y + i, l, gfx.color565(0xCF, 0x01, 0x00));
      gfx.writeFastHLine(x + 4 + l, y + i, w - 8 - l, BLACK);
    }
    gfx.endWrite();
  }
  delay(800);

  // 1982 Sinclair Research...
  gfx.fillScreen(0xd69a);
  gfx.draw16bitRGBBitmap(x, y, (uint16_t*)SinclairLtd_map, SinclairLtd_w, SinclairLtd_h);

  delay(2000);
}

// Draw the Speccy L cursor, flashing.
static void drawSpeccyCursor(int column, long startTime) {
  const bool isOn = (((millis() - startTime) / 320) % 2) == 0;
  const uint16_t* img = (uint16_t*)(isOn ? SpeccyL1_map : SpeccyL2_map);

  const int w = 192;
  const int h = 256;
  const int x = (gfx.width() - w) / 2;
  const int y = (gfx.height() - h) / 2;

  gfx.draw16bitRGBBitmap(x, y + column * 8, img, SpeccyL1_w, SpeccyL1_h);
}

// Set the Speccy border color.
static void speccyBorder(uint16_t rgb) {
  const int w = 192;
  const int h = 256;
  const int x = (gfx.width() - w) / 2;
  const int y = (gfx.height() - h) / 2;

  gfx.startWrite();
  for (int i = 0; i < gfx.width(); i++) {
    if (i < x || i > x + w) {
      // Top/bottom full-width lines.
      gfx.writeFastVLine(i, 0, gfx.height(), rgb);
    } else {
      // Middle section.
      gfx.writeFastVLine(i, 0, y, rgb);
      gfx.writeFastVLine(i, y + h, y, rgb);
    }
  }
  gfx.endWrite();
}

static void drawSpeccyLoadBars(uint16_t rgb1, uint16_t rgb2, int w1, int w2, long timeMs) {
    const int w = 192;
    const int h = 256;
    const int x = (gfx.width() - w) / 2;
    const int y = (gfx.height() - h) / 2;

    gfx.startWrite();
    for (int i = 0; i < gfx.width(); i++) {
      uint16_t rgb = (((i - timeMs / 10) & (w1 + w2)) >= w1) ? rgb1 : rgb2;

      if (i < x || i > x + w) {
        // Top/bottom full-width lines.
        gfx.writeFastVLine(i, 0, gfx.height(), rgb);
      } else {
        // Middle section.
        gfx.writeFastVLine(i, 0, y, rgb);
        gfx.writeFastVLine(i, y + h, y, rgb);
      }
    }
    gfx.endWrite();
}

static void runSpeccyLoad() {
  gfx.fillScreen(0xd69a);

  const int w = 192;
  const int h = 256;
  const int x = (gfx.width() - w) / 2;
  const int y = (gfx.height() - h) / 2;

  // LOAD
  gfx.startWrite();
  writeImageClipped(SpeccyLoad_map, SpeccyLoad_w, SpeccyLoad_h, x, y, 8 * 4);
  gfx.endWrite();

  // Flashy cursor.
  long t = millis(), start = t;
  while (millis() - t < 1000)
    drawSpeccyCursor(5, start);

  // ..."macos"
  for (int i = 1; i <= 7; i++) {
    gfx.startWrite();
    writeImageClipped(SpeccyLoad_map, SpeccyLoad_w, SpeccyLoad_h, x, y, 8 * (5 + i));
    gfx.endWrite();

    t = millis();
    while (millis() - t < 400)
      drawSpeccyCursor(5 + i, start);
  }

  t = millis();
  while (millis() - t < 1500)
    drawSpeccyCursor(5 + 7, start);

  // Program load sequence.
  gfx.fillScreen(0xd69a);
  for (int i = 0; i < 2; i++) {
    // Cyan/red flashy.
    speccyBorder(0x0679);
    delay(1000);
    speccyBorder(0xc800);
    delay(1000);

    // Cyan/red bars.
    t = millis();
    while (millis() - t < 3000)
      drawSpeccyLoadBars(0x0679, 0xc800, 6, 6, millis());

    gfx.draw16bitRGBBitmap(x + w - 14, y + 1, (uint16_t*)SpeccyProgram_map, SpeccyProgram_w, SpeccyProgram_h);
  }

  // Screen load sequence.
  t = millis();
  int rowsLoaded = 0;
  while (rowsLoaded < w) {
    // Yellow/Blue data bars.
    drawSpeccyLoadBars(0xce63, 0x0019, 3, 2, random());

    // B/W Screen load.
    int newRowsLoaded = (millis() - t) / 100;
    if (newRowsLoaded > rowsLoaded) {
      rowsLoaded++;

      int third = rowsLoaded / 64;
      int row8 = (rowsLoaded % 8) * 8;
      int rowOffset = (rowsLoaded / 8) % 8;

      gfx.startWrite();
      int xx = x + w - third * 64 - row8 - rowOffset;
      int tileX = xx;
      tileX %= MacTile_w;
      for (int yy = y; yy < y + h; yy++) {
        int tileY = yy % MacTile_h;
        uint16_t rgb = ((uint16_t*)MacTile_map)[tileY * MacTile_w + tileX];
        gfx.writePixel(xx, yy, (rgb & 0x1F) < 20 ? BLACK : 0xd69a);
      }
      gfx.endWrite();
    }
  }

  // Color fill-in (draw 8 columns at a time, right-to-left).
  speccyBorder(0xd69a);
  for (int i = 0; i < gfx.width(); i += 8) {
    gfx.startWrite();

    // Process a group of up to 8 columns.
    for (int dx = 0; dx < 8 && (i + dx) < gfx.width(); ++dx) {
      int x = gfx.width() - 1 - (i + dx);        // right-to-left screen x
      int tileX = x % MacTile_w;                 // tile column for this x
      for (int y = 0; y < gfx.height(); ++y) {
        int tileY = y % MacTile_h;
        uint16_t rgb = ((uint16_t*)MacTile_map)[tileY * MacTile_w + tileX];
        gfx.writePixel(x, y, rgb);
      }
    }

    gfx.endWrite();

    delay(75);
  }

  delay(2000);
}

static void runMacBoot() {
  // Background and splash.
  gfx.startWrite();
  writeImageTiled(MacTile_map, MacTile_w, MacTile_h);
  writeImageCentered(MacLogo_map, MacLogo_w, MacLogo_h);
  gfx.endWrite();

  // Progress bar.
  delay(800);
  const float durationMs = 3000.0f;
  const long startTime = millis();
  float p;
  while ((p = (millis() - startTime) / durationMs) <= 1.0f) {
    int w = 7 + int(59 * p);
    gfx.startWrite();
    writeImageStretched(MacProgress_map, MacProgress_w, MacProgress_h, (gfx.width() - MacLogo_w) / 2 + 9, (gfx.height() - MacLogo_h) / 2 + 65, w);
    gfx.endWrite();
  }

  delay(2000);

  // Desktop.
  gfx.startWrite();
  writeImageTiled(MacTile_map, MacTile_w, MacTile_h);
  gfx.endWrite();
}

void loop() {
  runSpeccyBoot();
  runSpeccyLoad();
  runMacBoot();

  delay(3000);
}