#pragma once

#ifndef SIM_MODE
#include "esp_heap_caps.h"
#endif

class SpeccyFB {
public:
    SpeccyFB() : _fb(nullptr) {}

    bool init() {
        _fb = (uint16_t*) heap_caps_malloc(256 * 192 * sizeof(uint16_t), MALLOC_CAP_INTERNAL);
        if (!_fb)
        return false;
        
        clear(0x0000);
        
        // Mark all dirty initially
        memset(_dirty, 1, sizeof(_dirty));
        for (int r = 0; r < 256; ++r) { _left[r] = 0; _right[r] = 191; }
        return true;
    }

    ~SpeccyFB() {
      if (_fb) free(_fb);
    }

    void startFrame() {
        memset(_dirty, 0, sizeof(_dirty));
        for (int r = 0; r < 256; ++r) { _left[r] = 192; _right[r] = -1; }
    }

    void clear(uint16_t c) {
        for (int i = 0; i < 256 * 192; ++i) _fb[i] = c;
        memset(_dirty, 1, sizeof(_dirty));
        for (int r = 0; r < 256; ++r) { _left[r] = 0; _right[r] = 191; }
    }

    // Logical plot: x∈[0,255], y∈[0,191]. Rotated at write time.
    void plot(int x, int y, uint16_t col) {
        if ((unsigned)x >= 256 || (unsigned)y >= 192) return;

        const int row = x;
        const int c   = 191 - y;               // stored column
        uint16_t* p   = _fb + row * 192 + c;
        if (*p != col) {
            *p = col;
            _dirty[row] = true;
            if (c < _left[row])  _left[row]  = c;
            if (c > _right[row]) _right[row] = c;
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
    
    /*
    void hline(int x, int y, int w, uint16_t col) {
        if ((unsigned)y >= 192 || w <= 0) return;
        if (x < 0) { w += x; x = 0; }
        if (x + w > 256) w = 256 - x;
        if (w <= 0) return;

        const int c = 191 - y;
        for (int i = 0; i < w; ++i) {
            const int row = x + i;
            _fb[row * 192 + c] = col;
            _dirty[row] = true;
            if (c < _left[row])  _left[row]  = c;
            if (c > _right[row]) _right[row] = c;
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
        _dirty[row] = true;

        // Span update: vertical line touches contiguous columns [191-(y+h-1) .. 191-y]
        const int c0 = 191 - (y + h - 1);
        const int c1 = 191 - y;
        if (c0 < _left[row])  _left[row]  = c0;
        if (c1 > _right[row]) _right[row] = c1;
    }
    */

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
            _dirty[rx] = true;
            if (c0 < _left[rx])  _left[rx]  = c0;
            if (c1 > _right[rx]) _right[rx] = c1;
        }
    }

    // Push only changed spans per dirty row. Assumes in-bounds blit.
    void endFrame(Arduino_ST7789& tft, int dstX, int dstY) {
        tft.startWrite();
        for (int row = 0; row < 256; ++row) {
            if (!_dirty[row]) continue;
            const int left  = _left[row];
            const int right = _right[row];
            if (left <= right) {
                const int w = right - left + 1;
                tft.writeAddrWindow(dstX + left, dstY + row, w, 1);
                tft.writePixels(_fb + row * 192 + left, w);
            }
            _dirty[row] = false;
        }
        tft.endWrite();
    }

private:
    uint16_t* _fb;
    bool      _dirty[256];
    int16_t   _left[256];
    int16_t   _right[256];
};