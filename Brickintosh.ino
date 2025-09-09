#ifndef SIM_MODE
#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include <Wire.h>
#endif

#include "Sprites.h"
#include "SpeccyFB.h"

/*****************************************************************************/

constexpr int16_t LCD_X_OFFSET = 0;    // Panel-specific vertical offset.
constexpr int16_t LCD_Y_OFFSET = 20;   // Panel-specific vertical offset.
static Arduino_ESP32SPI bus(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
static Arduino_ST7789 gfx(
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

static SpeccyFB speccy;

/*****************************************************************************/

// Set the brightness of a single RGB565 color.
static uint16_t scaleRgb565(uint16_t c, float f) {
  int r = (c >> 11) & 0x1F;
  int g = (c >> 5)  & 0x3F;
  int b =  c        & 0x1F;

  // When f=1 → original color, when f=2 → full white.
  float t = (f <= 1.0f) ? f : 1.0f;      // scaling part
  float w = (f > 1.0f) ? (f - 1.0f) : 0; // whitening part, 0..1

  r = int(r * t + (0x1F - r) * w + 0.5f);
  g = int(g * t + (0x3F - g) * w + 0.5f);
  b = int(b * t + (0x1F - b) * w + 0.5f);

  if (r > 0x1F) r = 0x1F;
  if (g > 0x3F) g = 0x3F;
  if (b > 0x1F) b = 0x1F;

  return (r << 11) | (g << 5) | b;
}

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

static HWCDC USBSerial;

void setup(void) {
  USBSerial.begin(115200);
  USBSerial.println("Brickintosh Demo by DeanTheCoder");
  USBSerial.println("https://github.com/deanthecoder/Brickintosh");

  // Init Display
  if (!gfx.begin()) {
    USBSerial.println("gfx.begin() failed. Halting.");
    while (1);
  }

  // Init the Spectrum frame buffer.
  if (!speccy.init()) {
    USBSerial.println("speccy.init() failed. Halting.");
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
  const float durationMs = 500.0f;
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

static void runMacMenu() {
  // Slide-on menu.
  for (int i = 0; i < MacMenu_w; i++) {
    gfx.draw16bitRGBBitmap(gfx.width() - i, 0, (uint16_t*)MacMenu_map, MacMenu_w, MacMenu_h);  
    delay(70);
  }
  delay(1000);

  // Demo window.
  long t = millis();
  float p;
  while ((p = (millis() - t) / 500.0f) < 1.0f) {
    p = int(p * 8) / 8.0f; // Discrete border sizes.
    int dx = int(MacWindow_w / 2 * p);
    int dy = int(MacWindow_h / 2 * p);
    gfx.drawRect(gfx.width() / 2 - dx, gfx.height() / 2 - dy, dx * 2, dy * 2, 0x9d13);
  }

  gfx.startWrite();
  writeImageCentered(MacWindow_map, MacWindow_w, MacWindow_h);
  gfx.endWrite();
}

static void runTunnel() {
  #define RINGS 32
  #define POINTS 24
  int16_t xy[RINGS][POINTS][2];

  // Calc the rings.
  for (int i = 0; i < RINGS; i++) {
    float z = i * ((128.0f * 80.0f / 20.0f) - 80.0f) / (RINGS - 1);
    float scale = 80.0f / (80.0f + z); // scale = focal / (focal + z)
    for (int j = 0; j < POINTS; j++) {
      float theta = (j * TWO_PI) / POINTS;
      xy[i][j][0] = sinf(theta) * 140.0f * scale;
      xy[i][j][1] = cosf(theta) * 140.0f * scale;
    }
  }

  // Draw the tunnel.
  long t = millis();
  int o = 0;
  float ringStart = 0.0f;
  while (true) {
    speccy.startFrame();
    speccy.clear(BLACK);

    for (int i = ringStart; i < RINGS; i++) {
      int bright = 16 * sinf((i + o * 0.6f) * 1.5f);
      if (bright <= 0)
        continue; // Hidden.

      // Smaller rings = darker.
      float f = (1.0f - float(i) / RINGS) * 2.0f;
      if (f > 1.5f) f = 1.5f;
      uint16_t c = scaleRgb565(RED, f);

      // Draw rings.
      int dx = 48 * cosf((o + i) * 0.03f);
      int dy = 32 * cosf((o + i) * 0.07f);

      for (int j = 0; j < POINTS; j++) {
        int x = 128 + xy[i][j][0] + dx;
        int y = 96 + xy[i][j][1] + dy;

        speccy.plot(x, y, c);
      }
    }

    speccy.endFrame(gfx, (gfx.width() - MacWindow_w) / 2 + 4, (gfx.height() - 256) / 2);

    o++;

    // Finished?
    if (millis() - t > 8000)
    {
      ringStart += 0.2f;
      if (ringStart > RINGS)
        break;  // We're done.
    }
  }
}

static void runQR() {
  float p;

  // Scale to max.
  long t = millis();
  while ((p = (millis() - t) / 3000.0) <= 1.0) {
    float s = p * p * (3.0f - 2.0f * p);   // smoothstep(0,1,p)
    speccy.startFrame();
    speccy.writeImageScaled((uint16_t*)QR_map, QR_w, QR_h, 256 / 2, 192 / 2, s);
    speccy.endFrame(gfx, (gfx.width() - MacWindow_w) / 2 + 4, (gfx.height() - 256) / 2);
  }

  delay(4000);
}

static void runQrToWave() {
  // QR code → wave particle animation.
  // 16x16 particles sample the QR image, morph to a cosine wave,
  // flatten to a line, then fly off-screen left/right.

  // Particle grid params.
  constexpr int PG = 16;               // particles per axis (PG x PG)
  constexpr int PN = PG * PG;          // total particles

  // Speccy framebuffer center.
  const int cx = 256 / 2;
  const int cy = 192 / 2;

  // Precompute particles from QR image, centered on speccy buffer.
  struct Particle {
    int16_t sx, sy;     // start (QR) position in speccy coords
    uint16_t color;     // particle color
    int8_t dir;         // -1 = left, +1 = right for fly-away
    bool active;        // skip if false (e.g., black QR cell)
    uint8_t gx, gy;     // grid coordinates for wave param
  };
  static Particle P[PN];  // avoid large stack usage

  int idx = 0;
  for (int gy = 0; gy < PG; ++gy) {
    // Sample y in QR image (center of each cell)
    int syImg = (QR_h * (2 * gy + 1)) / (2 * PG);
    for (int gx = 0; gx < PG; ++gx, ++idx) {
      int sxImg = (QR_w * (2 * gx + 1)) / (2 * PG);
      uint16_t c = ((uint16_t*)QR_map)[syImg * QR_w + sxImg];

      // Initialize particle
      Particle &pp = P[idx];
      pp.gx = (uint8_t)gx;
      pp.gy = (uint8_t)gy;
      pp.color = c;
      pp.active = (c != BLACK);     // skip black samples for speed/clarity
      pp.dir = (gx < PG / 2) ? -1 : +1;

      // Map QR image coordinates into speccy space, centered.
      pp.sx = cx - (QR_w / 2) + sxImg;
      pp.sy = cy - (QR_h / 2) + syImg;
    }
  }

  // Animation timing (ms)
  const long t0 = millis();
  const float morphDur = 3400.0f;   // QR → wave morph
  const float waveDur  = 4000.0f;   // oscillate while flattening
  const float flyDur   = 1200.0f;   // fly off-screen
  const float totalDur = morphDur + waveDur + flyDur;

  // Wave params
  const float baseAmp  = 56.0f;     // initial amplitude in pixels
  const float phaseHz  = 0.7f;      // wave phase speed (cycles/sec)

  // Precompute per-column x→theta, x→tx and per-row multipliers
  float thetaLut[PG];
  int   txLut[PG];
  float rowMul[PG];
  for (int x = 0; x < PG; ++x) {
    float u = (PG == 1) ? 0.0f : (float)x / (float)(PG - 1);
    thetaLut[x] = u * TWO_PI;
    txLut[x] = (int)(u * 255.0f + 0.5f);
  }
  for (int y = 0; y < PG; ++y) rowMul[y] = 1.0f + 0.18f * (float)y;

  // Particle visual size.
  const int halfWH = 4;

  // Render frames until animation completes.
  long now;
  while ((now = millis()) - t0 < (long)totalDur + 60) {
    float t = (float)(now - t0);

    // Stage progress
    float morphP = t < morphDur ? (t / morphDur) : 1.0f;               // 0..1
    float waveP  = (t <= morphDur) ? 0.0f : ((t - morphDur) / waveDur);
    if (waveP > 1.0f) waveP = 1.0f;
    float flyP   = (t <= morphDur + waveDur) ? 0.0f : ((t - morphDur - waveDur) / flyDur);
    if (flyP > 1.0f) flyP = 1.0f;

    float mp = morphP * morphP * (3.0f - 2.0f * morphP);
    float phase = TWO_PI * phaseHz * (t / 1000.0f);

    float amp = baseAmp * (1.0f - waveP);

    speccy.startFrame();
    speccy.clear(BLACK);

    // Draw particles
    for (int i = 0; i < PN; ++i) {
      const Particle &pp = P[i];
      if (!pp.active) continue;

      const int tx = txLut[pp.gx];
      const float theta = thetaLut[pp.gx];
      int ty = cy + (int)(amp * cosf(theta * rowMul[pp.gy] + phase));

      // Interpolate from QR sample pos → wave target pos during morph.
      int x = pp.sx + (int)((tx - pp.sx) * mp);
      int y = pp.sy + (int)((ty - pp.sy) * mp);

      // During fly phase, move horizontally off-screen from the line center.
      if (flyP > 0.0f) {
        // Ease-in for fly-away for a snappier exit.
        float fp = flyP * flyP;
        // Move up to ~1.5 screen widths so particles disappear.
        x += (int)(pp.dir * fp * 384.0f);
        // Flatten y fully by fly time.
        y = cy;
      }

      // Draw a filled rectangle roughly sized to QR tile
      speccy.fillRect(x - halfWH, y - halfWH, halfWH << 1, halfWH << 1, pp.color);
    }

    speccy.endFrame(gfx, (gfx.width() - MacWindow_w) / 2 + 4, (gfx.height() - 256) / 2);
  }
}

static void runFire() {
  constexpr int W = 128;                  // logical fire columns
  constexpr int H = 96;                  // limit height for speed
  constexpr int TOP_Y = 192 - H;        // top of fire region on speccy

  static uint8_t buf[H][W];             // intensity 0..255
  uint16_t pal[256];             // RGB565 palette

  for (int i = 0; i < 256; ++i) {
    int r = 0, g = 0, b = 0;
    if (i < 64) {                     // black -> red
      r = i << 2;
    } else if (i < 128) {             // red -> orange
      r = 255; g = (i - 64) << 2;
    } else if (i < 192) {             // orange -> yellow
      r = 255; g = 255;
    } else {                          // yellow -> white
      r = 255; g = 255; b = (i - 192) << 2; if (b > 255) b = 255;
    }
    pal[i] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
  }

  // Clear buffer
  memset(buf, 0, sizeof(buf));

  // Base line randoms.
  uint8_t seeds[W];
  for (int x = 0; x < W; ++x)
    seeds[x] = (uint8_t)random();

  const long t0 = millis();
  while (millis() - t0 < 20000) {
    long frameStart = millis();

    // Seed bottom row with noise, based on sine wave + random
    for (int x = 0; x < W; ++x)
      buf[H - 1][x] = (uint8_t)(155 + 100 * sinf((frameStart + seeds[x]) * seeds[x] / 256.0f * 0.01f));

    // Propagate upwards (compute from bottom-2 up to top)
    for (int y = 0; y < H - 1; y++) {
      const uint8_t* nextRow = buf[y + 1];
      const uint8_t* nextNextRow = (y + 2 < H) ? buf[y + 2] : nextRow;
      uint8_t* row = buf[y];

      row[0] = 0;  // edges cool fast
      for (int x = 1; x < W - 1; ++x) {
        int sum = nextRow[x - 1] + nextRow[x] + nextRow[x + 1] + nextNextRow[x];
        row[x] = (uint8_t)(sum * 32 / 132);
      }
      row[W - 1] = 0;
    }

    // Draw to speccy framebuffer.
    speccy.startFrame();
    for (int y = 0; y < H - 1; ++y) {
      int sy = (y + 1) * 2;
      for (int x = 0; x < W; ++x) {
        uint16_t c = pal[buf[y][x]];
        speccy.fillRect(x * 2, sy, 2, 2, c);
      }
    }

    speccy.endFrame(gfx, (gfx.width() - MacWindow_w) / 2 + 4, (gfx.height() - 256) / 2);

    // Limit FPS
    long dt = millis() - frameStart;
    const long frameMs = 1000 / 20;
    if (dt < frameMs) delay(frameMs - dt);
  }
}

static void runAmigaBall() {
  const float invPi = 1.0f / M_PI;
  const int ballRadius = 40;
  const int ballR2 = ballRadius * ballRadius;
  int ballX = 128;
  int ballY = 64;
  int ballDx = 1;
  float ballDy = 0.0f;         // Use float for smoother motion
  float ballAy = 0.35f;        // Gravity acceleration (pixels/frame^2)
  int ballDUv = 0;

  long t0 = millis();
  while (millis() - t0 < 20000) {
    long frameStart = millis();

    // Update ball X position.
    if (ballX + ballDx * 3 - ballRadius < 0)
      ballDx = 1;
    else if (ballX + ballDx * 3 + ballRadius >= 256)
      ballDx = -1;
    ballX += ballDx * 2;
    ballDUv += (ballDx > 0) ? 1 : -1; // Spin the ball

    // Update ball Y position (under gravity).
    ballDy += ballAy;           // Apply gravity
    ballY += (int)ballDy;       // Move ball

    // Bounce off floor
    if (ballDy > 0 && ballY + ballRadius >= 152) {
      ballY = 152 - ballRadius;
      ballDy = -ballDy * 1.05f;
    }

    // Clear frame.
    speccy.startFrame();
    speccy.clear(0xad55);

    // Draw ball shadow.
    for (int dx = -ballRadius; dx <= ballRadius; ++dx) {
      int nx2 = dx * dx;

      for (int dy = -ballRadius; dy <= ballRadius; ++dy) {
        if (nx2 + dy * dy < ballR2)
          speccy.plot(ballX + dx + 16, ballY + dy, 0x632c);
      }
    }

    // Draw background.
    for (int y = 0; y <= 144; y += 12)
      speccy.hline(56, y + 2, 144, 0xa815);
    for (int x = 0; x <= 144; x += 12) {
      speccy.vline(56 + x, 2, 144, 0xa815);
      speccy.writeLine(56 + x, 146, 40 + x + x * 32 / 144, 156, 0xa815);
    }
    for (int i = 0, ii = 2; i < 4; i++, ii += i + 1) {
      speccy.hline(56 - ii * 3 / 2, 146 + ii, 145 + ii * 3, 0xa815);
    }

    // Draw ball.
    for (int dx = -ballRadius; dx <= ballRadius; ++dx) {
      float nx = (float)dx / (float)ballRadius; // -1..1
      float nx2 = nx * nx;

      for (int dy = -ballRadius; dy <= ballRadius; ++dy) {
        float ny = (float)dy / (float)ballRadius; // -1..1
        float r2 = nx2 + ny * ny;

        if (r2 <= 1.0f) {
          // Calculate z for the sphere surface
          float nz = sqrtf(1.0f - r2);

          int x = ballX + dx;
          int y = ballY + dy;

          // Environment mapping: use normal to get UV
          float u = 0.5f + atan2f(nx, nz) * invPi;
          float v = 0.5f - asinf(ny) * invPi;

          u += ballDUv * 0.05f;

          // Rotate u/v by 45 degrees.
          float u2 = u - 0.5f;
          float v2 = v - 0.5f;
          float s = sinf(0.25f); // 45 degrees
          float c = cosf(0.25f);
          u = (u2 * c - v2 * s) + 50.5f;
          v = (u2 * s + v2 * c) + 50.5f;

          // Checkerboard pattern
          int checkU = int(u * 8.0f);
          int checkV = int(v * 8.0f);
          speccy.plot(x, y, ((checkU + checkV) & 1) == 0 ? WHITE : RED);
        }
      }
    }

    speccy.endFrame(gfx, (gfx.width() - MacWindow_w) / 2 + 4, (gfx.height() - 256) / 2);

    // Limit FPS
    long dt = millis() - frameStart;
    const long frameMs = 1000 / 20;
    if (dt < frameMs) delay(frameMs - dt);
  }
}

static void runDonut() {
  // ASCII donut adapted to SpeccyFB using 2x2 filled pixels + simple z-buffer.
  // Based on the classic a1k0n donut math, but shaded by luminance into RGB565.
  // Logical render size is 128x96 (maps to 256x192 by drawing 2x2 blocks).

  // Tunables
  const float THETA_SPACING = 0.04f;
  const float PHI_SPACING   = 0.01f;
  const float R1 = 0.7f;           // small radius
  const float R2 = 2.0f;           // big radius
  const float K2 = 5.0f;           // distance from viewer
  const int   LW = 128;            // logical width  (×2 -> 256)
  const int   LH = 96;             // logical height (×2 -> 192)

  // Colors
  const uint16_t BG     = gfx.color565(26, 28, 44);    // deep blue-ish background
  const uint16_t DONUT0 = gfx.color565(255, 120, 16);  // warm orange

  // k1 matches a1k0n's derivation (scaled for our logical width)
  const float k1 = (float)LW * K2 * 2.0f / (8.0f * (R1 + R2));

  // Simple animation timing
  const long t0 = millis();
  const long DURATION_MS = 20000;   // run for 20 seconds
  const float A_SPEED = 0.9f;       // radians/sec
  const float B_SPEED = 0.35f;      // radians/sec

  // Z-buffer (ooz) for logical pixels
  static float zbuf[LW][LH];

  while (millis() - t0 < DURATION_MS) {
    long frameStart = millis();

    // Compute rotation angles from time for smoothness
    float tsec = (frameStart - t0) / 1000.0f;
    float a = 0.2f + A_SPEED * tsec;
    float b = 0.1f + B_SPEED * tsec;

    // Precompute sin/cos of A/B
    const float cosA = cosf(a), sinA = sinf(a);
    const float cosB = cosf(b), sinB = sinf(b);

    // Clear z-buffer
    memset(zbuf, 0, sizeof(zbuf));
    
    // Begin frame
    speccy.startFrame();
    speccy.clear(BG);

    // Theta loop
    for (float theta = 0.0f; theta < TWO_PI; theta += THETA_SPACING) {
      float cosT = cosf(theta), sinT = sinf(theta);

      // Factors reused across phi
      float f1 = sinB * cosT;
      float f2 = cosA * cosT;
      float f3 = sinA * sinT;
      float f4 = cosA * sinT;
      float f5 = sinA * cosT;

      float circleX = R2 + R1 * cosT;
      float circleY = R1 * sinT;

      // Phi loop
      for (float phi = 0.0f; phi < TWO_PI; phi += PHI_SPACING) {
        float cosP = cosf(phi), sinP = sinf(phi);

        // Luminance (point-light at viewer)
        float L = cosP * f1 - f2 * sinP - f3 + cosB * (f4 - f5 * sinP);
        if (L < 0.05f) L = 0.05f;

        // 3D → camera
        const float x = circleX * (cosB * cosP + sinA * sinB * sinP) - circleY * cosA * sinB;
        const float y = circleX * (sinB * cosP - sinA * cosB * sinP) + circleY * cosA * cosB;
        const float z = K2 + cosA * circleX * sinP + circleY * sinA;
        const float ooz = 1.0f / z;

        // Projection to logical coords
        const int xp = (int)(LW / 2.0f + k1 * ooz * x);
        const int yp = (int)(LH / 2.0f - k1 * ooz * y);

        // Z-test
        if (ooz > zbuf[xp][yp]) {
          zbuf[xp][yp] = ooz;

          // Shade donut color.
          const float shade = 0.4f + 0.8f * (L > 1.0f ? 1.0f : L);
          const uint16_t c = scaleRgb565(DONUT0, shade);

          // Draw voxel.
          speccy.fillRect(xp * 2, yp * 2, 2, 2, c);
        }
      }
    }

    // Blit to the screen inside the Mac window area (same placement as other effects)
    speccy.endFrame(gfx,
                    (gfx.width() - MacWindow_w) / 2 + 4,
                    (gfx.height() - 256) / 2);

    // Cap frame rate ~20 fps
    long dt = millis() - frameStart;
    const long frameMs = 1000 / 20;
    if (dt < frameMs) delay(frameMs - dt);
  }
}

static void runRain() {
  struct Glyphs {
    int16_t y;      // position in speccy text coords
    uint8_t length; // length in glyphs
  };
  constexpr int MAX_GLYPHS = 32;
  Glyphs glyphs[MAX_GLYPHS];

  // Initialize glyph runs.
  for (int i = 0; i < MAX_GLYPHS; ++i) {
    glyphs[i].length = 5 + (random() % 10);
    glyphs[i].y = (random() % 24) - glyphs[i].length;
  }

  long t0 = millis();
  while (millis() - t0 < 20000) {
    long frameStart = millis();

    // Update glyph positions.
    for (int i = 0; i < MAX_GLYPHS; ++i) {
      Glyphs &g = glyphs[i];
      g.y++;
      if (g.y > 24) {
        g.length = 5 + (random() % 10);
        g.y = -g.length;
      }
    }

    // Render frame
    speccy.startFrame();
    speccy.clear(BLACK);

    for (int i = 0; i < MAX_GLYPHS; ++i) {
      Glyphs &g = glyphs[i];
      int sx = i * 8;

      for (int j = 0; j < g.length; j++) {
        int glyphIndex = random() % 26;
        float brightness = (j + 1.0f) / g.length;

        // Darken every other column.
        brightness *= (i & 1) ? 0.3f : 1.0f;

        // Brighten last glyph.
        if (j == g.length - 1)
          brightness *= 1.5f;

        // Draw glyph (8x8 pixels)
        for (int y = 0; y < 8; ++y) {
          int sy = (g.y + j) * 8 + y;
          if (sy < 0) continue; // off top of screen
          if (sy >= 192) break; // off bottom of screen

          uint16_t* img = speccy.getSpriteSheetTileRow((uint16_t*)Rain_map, Rain_W, 8, 8, glyphIndex, y);
          for (int x = 0; x < 8; ++x) {
            uint16_t c = img[x];
            if (c != BLACK) {
              // Scale color by brightness.
              int green = (c >> 5) & 0x3F;

              c = scaleRgb565(0x0768, brightness * green / 32.0f);
              speccy.plot(sx + x, sy, c);
            }
          }
        }
      }
    }
    
    speccy.endFrame(gfx, (gfx.width() - MacWindow_w) / 2 + 4, (gfx.height() - 256) / 2);

    // Cap frame rate.
    long dt = millis() - frameStart;
    const long frameMs = 1000 / 8;
    if (dt < frameMs) delay(frameMs - dt);
  }
}

void loop() {
  runSpeccyBoot();
  runSpeccyLoad();
  runMacBoot();
  runMacMenu();
  runTunnel();
  runQR();
  runQrToWave();
  runFire();
  runAmigaBall();
  runDonut();
  runRain();

  delay(3000);
}
