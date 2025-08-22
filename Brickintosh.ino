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
    1,            // rotation (90 degrees)
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

// Stretch an RGB565 image horizontally to a target width by duplicating
// the first source column to the left, and drawing the original image on the right.
// Assumes caller wraps with startWrite()/endWrite().
// img: pointer to w*h pixels, RGB565 packed in a uint8_t buffer
// w,h: source image dimensions
// x,y: top-left of the stretched bar area on screen
// targetW: total width of the stretched area in pixels
static inline void writeImageStretched(const uint8_t* img, int w, int h, int x, int y, int targetW)
{
  const int leftFillW = (targetW > w) ? (targetW - w) : 0;

  // 1) Fill the left area using the first column of the image
  if (leftFillW > 0)
  {
    for (int row = 0; row < h; ++row)
    {
      // First pixel of row (2 bytes per pixel)
      uint16_t col = *(uint16_t*)(img + row * w * 2);
      gfx.writeFastHLine(x, y + row, leftFillW, col);
    }
  }

  // 2) Draw the original image so its RHS aligns with x + targetW - 1
  //    i.e. its left starts at x + leftFillW
  gfx.writeAddrWindow(x + leftFillW, y, w, h);
  gfx.writePixels((uint16_t*)img, w * h);
}

// Draw up to `width` pixels from each row of the source image.
// Assumes caller wraps with startWrite()/endWrite().
// img: pointer to w*h pixels, RGB565 packed in a uint8_t buffer
// w,h: source image dimensions
// x,y: top-left target position on screen
// width: number of columns from the source image to draw (<= w)
static inline void writeImageClipped(const uint8_t* img, int w, int h, int x, int y, int width)
{
  if (width > w) width = w;

  for (int row = 0; row < h; ++row)
  {
    const uint8_t* srcLine = img + row * (w * 2); // start of row
    gfx.writeAddrWindow(x, y + row, width, 1);
    gfx.writePixels((uint16_t*)srcLine, width);
  }
}

/*****************************************************************************/

HWCDC USBSerial;

void setup(void) {
  USBSerial.begin(115200);
  USBSerial.println("Brickintosh Demo by DeanTheCoder");
  USBSerial.println("https://github.com/deanthecoder/Brickintosh");

  int numCols = LCD_WIDTH / 8;
  int numRows = LCD_HEIGHT / 10;

  // Init Display
  if (!gfx.begin()) {
    USBSerial.println("gfx.begin() failed, halting.");
    while (1);
  }

  gfx.fillScreen(BLACK);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
}

static void runSpeccyBoot() {
  gfx.fillScreen(gfx.color565(0xCF, 0xCF, 0xCF));
  delay(1000);

  // Black flash.
  const int w = 256;
  const int h = 192;
  const int x = (gfx.width() - w) / 2;
  const int y = (gfx.height() - h) / 2;
  gfx.fillRect(x, y, w, h, BLACK);
  delay(800);

  // Shrinking red bars.
  const float durationMs = 250.0f;
  const long startTime = millis();
  float p;
  while ((p = (millis() - startTime) / durationMs) <= 1.0f) {
    // Lines go from small to big to small again.
    p *= 2.0f;
    if (p > 1.0f)
      p = 2.0f - p;

    gfx.startWrite();
    for (int i = 8; i < 256; i += 8) {
      int l = int(p * (h - 8));

      // Clear lines.
      gfx.writeFastVLine(x + i, y, h, BLACK);

      // Draw red.
      gfx.writeFastVLine(x + i, y + h - 4 - l, l, gfx.color565(0xCF, 0x01, 0x00));
    }
    gfx.endWrite();
  }
  delay(800);

  // 1982 Sinclair Research...
  gfx.fillScreen(gfx.color565(0xCF, 0xCF, 0xCF));
  gfx.draw16bitRGBBitmap(x, y + h - SinclairLtd_h, (uint16_t*)SinclairLtd_map, SinclairLtd_w, SinclairLtd_h);
  delay(2000);
}

static void runMacBoot() {
  // Background and splash.
  gfx.startWrite();
  writeImageTiled(MacTile_map, MacTile_w, MacTile_h);
  writeImageCentered(MacLogo_map, MacLogo_w, MacLogo_h);
  gfx.endWrite();

  // Progress bar.
  const float durationMs = 3000.0f;
  const long startTime = millis();
  float p;
  while ((p = (millis() - startTime) / durationMs) <= 1.0f) {
    int w = 7 + int(59 * p);
    gfx.startWrite();
    writeImageStretched(MacProgress_map, MacProgress_w, MacProgress_h, (gfx.width() - MacLogo_w) / 2 + 65, (gfx.height() - MacLogo_h) / 2 + 131, w);
    gfx.endWrite();
  }

  delay(1000);
}

void loop() {
  gfx.fillScreen(BLACK);
  delay(500);

  runSpeccyBoot();
  runMacBoot();
}