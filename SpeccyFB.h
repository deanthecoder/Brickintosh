#pragma once

#ifndef SIM_MODE
#include "esp_heap_caps.h"
#endif

class SpeccyFB {
public:
    SpeccyFB() : _fb(nullptr), _background(0xFFFF) {}

    bool init() {
        _fb = (uint16_t*) heap_caps_malloc(256 * 192 * sizeof(uint16_t), MALLOC_CAP_INTERNAL);
        if (!_fb)
            return false;
        
        clear(0x0000);
        return true;
    }

    ~SpeccyFB() {
      if (_fb) free(_fb);
    }

    void startFrame() {
        memset(_dirtyCur, 0, sizeof(_dirtyCur));
    }

    void clear(uint16_t c) {
        _background = c;
        for (int i = 0; i < 256 * 192; ++i) _fb[i] = c;

        memset(_dirtyCur, 1, sizeof(_dirtyCur));
        memset(_dirtyPrev, 1, sizeof(_dirtyPrev));
    }

    // Logical plot: x∈[0,255], y∈[0,191]. Rotated at write time.
    void plot(int x, int y, uint16_t col) {
        if ((unsigned)x >= 256 || (unsigned)y >= 192) return;

        const int row = x;
        const int c   = 191 - y;               // stored column
        uint16_t* p   = _fb + row * 192 + c;
        if (*p == col) return; // no change
        *p = col;
        _dirtyCur[row] = true;
    }

    // writeLine(x, y, x2, y2, col)
    void writeLine(int x0, int y0, int x1, int y1, uint16_t col) {
        int dx = x1 - x0;
        int dy = y1 - y0;
        int sx = (dx >= 0) ? 1 : -1;
        int sy = (dy >= 0) ? 1 : -1;
        dx = sx * dx; // abs(dx)
        dy = sy * dy; // abs(dy)
        int err = dx - dy;
        for (;;) {
            plot(x0, y0, col);
            if (x0 == x1 && y0 == y1) break;
            int e2 = err << 1;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 <  dx) { err += dx; y0 += sy; }
        }
    }

    // Draw an RGB565 image scaled and centered at (cx, cy) in logical 256x192 space.
    // scale: 0..1.0  (0 => nothing, 1 => full size)
    void writeImageScaled(const uint16_t* img, int srcW, int srcH, int cx, int cy, float scale) {
        // Destination size (rounded)
        const int dstW = int(srcW * scale + 0.5f);
        const int dstH = int(srcH * scale + 0.5f);

        // Top-left so the image is centered at (cx, cy)
        const int x0 = cx - (dstW >> 1);
        const int y0 = cy - (dstH >> 1);

        // Nearest-neighbour sampling with integer math (rounded)
        // sx = round(dx * srcW / dstW), sy = round(dy * srcH / dstH)
        for (int dy = 0; dy < dstH; ++dy) {
            const int sy = (dy * srcH + (dstH >> 1)) / dstH;
            const int y  = y0 + dy;
            const uint16_t* srcRow = img + sy * srcW;

            for (int dx = 0; dx < dstW; ++dx) {
                const int sx = (dx * srcW + (dstW >> 1)) / dstW;
                const int x  = x0 + dx;
                plot(x, y, srcRow[sx]);
            }
        }
    }
    
    void hline(int x, int y, int w, uint16_t col) {
        if ((unsigned)y >= 192 || w <= 0) return;
        if (x < 0) { w += x; x = 0; }
        if (x + w > 256) w = 256 - x;
        if (w <= 0) return;

        const int c = 191 - y;
        for (int i = 0; i < w; ++i) {
            const int row = x + i;
            _fb[row * 192 + c] = col;
            _dirtyCur[row] = true;
        }
    }

    void vline(int x, int y, int h, uint16_t col) {
        if ((unsigned)x >= 256 || h <= 0) return;
        if (y < 0) { h += y; y = 0; }
        if (y + h > 192) h = 192 - y;
        if (h <= 0) return;

        const int row = x;
        uint16_t* p = _fb + row * 192 + (191 - y);
        for (int j = 0; j < h; ++j) { *p-- = col; }
        _dirtyCur[row] = true;
    }

    // Draw a filled rect in logical space (handy + updates spans correctly).
    void fillRect(int x, int y, int w, int h, uint16_t col) {
        if (w <= 0 || h <= 0) return;
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > 256) w = 256 - x;
        if (y + h > 192) h = 192 - y;
        if (w <= 0 || h <= 0) return;

        // Each logical x is a stored row; y-span becomes a contiguous column span.
        const int c0 = 191 - (y + h - 1);
        const int c1 = 191 - y;
        for (int rx = x; rx < x + w; ++rx) {
            uint16_t* p = _fb + rx * 192 + c1;
            for (int c = c1; c >= c0; --c) { *p-- = col; }
            _dirtyCur[rx] = true;
        }
    }

    /// Push only changed spans per dirty row. Assumes in-bounds blit.
    /// \param clearAfter If true, rows that were dirty in the previous frame are also pushed.
    ///        This ensures shrinking/erasing objects get cleared on the panel even if the new
    ///        frame doesn't touch those rows. After presenting, the framebuffer can optionally
    ///        be cleared back to the background (see code below).
    ///        If false, only rows touched this frame are sent; the FB is left intact.
    void endFrame(Arduino_ST7789& tft, int dstX, int dstY, bool clearAfter) {
        tft.startWrite();
        int row = 0;
        while (row < 256) {
            // Find next dirty run
            while (row < 256 && !(_dirtyCur[row] || (clearAfter && _dirtyPrev[row]))) ++row;
            if (row >= 256) break;

            int start = row;
            int end = row;
            ++row;
            while (row < 256 && (_dirtyCur[row] || (clearAfter && _dirtyPrev[row]))) { end = row; ++row; }

            const int h = end - start + 1;
            tft.writeAddrWindow(dstX, dstY + start, 192, h);
            tft.writePixels(_fb + start * 192, 192 * h);

            if (clearAfter) {
                for (int r = start; r <= end; ++r) {
                    uint16_t* p = _fb + r * 192;
                    for (int i = 0; i < 192; ++i) p[i] = _background;
                }
            }
        }
        memcpy(_dirtyPrev, _dirtyCur, sizeof(_dirtyCur));
        memset(_dirtyCur, 0, sizeof(_dirtyCur));
        tft.endWrite();
    }

    // Get a pointer to a specific row of a specific tile in a sprite sheet.
    uint16_t* getSpriteSheetTileRow(uint16_t* sheet, int sheetW, int tileW, int tileH, int tileIndex, int pixelRow) {
        const int tilesPerRow = sheetW / tileW;
        const int tileX = (tileIndex % tilesPerRow) * tileW;
        const int tileY = (tileIndex / tilesPerRow) * tileH;
        return sheet + (tileY + pixelRow) * sheetW + tileX;
    }

private:
    uint16_t* _fb;
    uint16_t  _background;
    bool      _dirtyCur[256];  // rows touched this frame
    bool      _dirtyPrev[256]; // rows touched last frame (used for shrink/erase)
};
