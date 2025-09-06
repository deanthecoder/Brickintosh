#pragma once

#include <cstdint>
#include <chrono>
#include <thread>

extern "C" unsigned int sleep(unsigned int seconds);
extern "C" void sdlLoop();

#define heap_caps_malloc(size, caps) malloc(size)
#define LCD_DC 0
#define LCD_CS 0
#define LCD_SCK 0
#define LCD_MOSI 0
#define LCD_RST 0
#define LCD_WIDTH 240
#define LCD_HEIGHT 280

#define TWO_PI 6.28318530718

#define BLACK 0
#define WHITE 0xFFFF
#define RED 0xF800

#define pinMode(a, b)
#define digitalWrite(a, b)
#define random() rand()

typedef struct Arduino_ESP32SPI
{
    int m_dc;
    int m_cs;
    int m_sclk;
    int m_mosi;

    Arduino_ESP32SPI(int dc, int cs, int sclk, int mosi) : 
        m_dc(dc), m_cs(cs), m_sclk(sclk), m_mosi(mosi) {}
    
} Arduino_ESP32SPI;

class Arduino_ST7789
{
private:
    int m_windowLeft{};
    int m_windowTop{};
    int m_windowWidth{};
    int m_windowHeight{};

public:
    uint16_t buffer[280*240] = {0}; // your RGB565 framebuffer
    
    Arduino_ST7789(void* bus, int d1, int d2, int d3, int d4, int d5, int d6, int d7, int d8, int d9)
    {
    }

    bool begin()
    {
        return true;
    }
    
    int width() { return LCD_WIDTH; }
    int height() { return LCD_HEIGHT; }

    void startWrite()
    {
    }

    void endWrite()
    {
        sdlLoop();
    }
    
    // Converts RGB888 color components to RGB565 format
    // r, g, b values should be in range 0-255
    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

    void writeAddrWindow(int l, int t, int w, int h)
    {
        m_windowLeft = l;
        m_windowTop = t;
        m_windowWidth = w;
        m_windowHeight = h;
    }

    void writePixel(int16_t x, int16_t y, uint16_t color) {
        if ((x < 0) || (x >= LCD_WIDTH) || (y < 0) || (y >= LCD_HEIGHT)) return;
        buffer[x + y * LCD_WIDTH] = color;
    }

    void writePixels(uint16_t* pixels, int count)
    {
        int x = m_windowLeft;
        int y = m_windowTop;
        int w = m_windowWidth;
        int h = count / w;
        
        for (int row = 0; row < h; row++) {
            for (int col = 0; col < w; col++) {
                writePixel(x + col, y + row, *pixels);
                pixels++;
            }
        }
    }
    
    void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
    {
        if ((x < 0) || (x >= LCD_WIDTH) || (h <= 0)) return;
        if (y < 0) { h += y; y = 0; }
        if ((y + h) > LCD_HEIGHT) h = LCD_HEIGHT - y;

        uint16_t* p = buffer + x + y * LCD_WIDTH;
        while (h--) {
            *p = color;
            p += LCD_WIDTH;
        }
    }
    
    void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
    {
        if ((y < 0) || (y >= LCD_HEIGHT) || (w <= 0)) return;
        if (x < 0) { w += x; x = 0; }
        if ((x + w) > LCD_WIDTH) w = LCD_WIDTH - x;

        uint16_t* p = buffer + x + y * LCD_WIDTH;
        while (w--) {
            *p++ = color;
        }
    }

    void fillScreen(uint16_t color)
    {
        for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
            buffer[i] = color;
        }
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
    {
        if (w <= 0 || h <= 0) return;
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
        if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
        if (w <= 0 || h <= 0) return;

        uint16_t* p = buffer + x + y * LCD_WIDTH;
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                p[i] = color;
            }
            p += LCD_WIDTH;
        }

        sdlLoop();
    }

    void draw16bitRGBBitmap(int16_t x, int16_t y, const uint16_t* bitmap, int16_t w, int16_t h) 
    {
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                writePixel(x + i, y + j, bitmap[j * w + i]);
            }
        }

        sdlLoop();
    }

    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        writeFastHLine(x, y, w, color);
        writeFastHLine(x, y+h-1, w, color);
        writeFastVLine(x, y, h, color);
        writeFastVLine(x+w-1, y, h, color);
    }
};

class HWCDC
{
public:
    void begin(int dummy)
    {
    }

    void println(const char* dummy)
    {
    }
};

uint64_t millis()
{
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    auto now = steady_clock::now();
    return duration_cast<milliseconds>(now - start).count();
}

void delay(uint64_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}