[![Twitter URL](https://img.shields.io/twitter/url/https/twitter.com/deanthecoder.svg?style=social&label=Follow%20%40deanthecoder)](https://twitter.com/deanthecoder)

# 🧱 Brickintosh

A tiny Lego Macintosh powered by an ESP32-S3 + 1.69" LCD display.  
It looks like a Mac, boots as a ZX Spectrum, loads MacOS 9, and occasionally throws a kernel panic just to keep you guessing.  
Because why settle for one retro system when you can mash them all together?

---

## 🛠 Hardware

- [ESP32-S3 Dev Board with 1.69" IPS ST7789 LCD](https://thepihut.com/products/esp32-s3-development-board-with-1-69-lcd-display-240-x-280)  
- Lego bricks (to taste) for the iconic case.  

---

## 📦 Software

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) for LCD driving.  
- RGB565-encoded visuals.  

---

## 🖥 ESP_SIM

`ESP_SIM` is a simple `main.cpp` file that uses [SDL2](https://www.libsdl.org/) to emulate the output of the ESP32 device. This allows testing and code iterations quickly and easily on the desktop, before deploying to the actual hardware.

---

## 🚀 Build & Run

### ESP32

Build and flash the firmware using the Arduino IDE or PlatformIO with VS Code. Connect your ESP32-S3 device and upload the code directly to the hardware.

### ESP_SIM

To run the ESP_SIM emulator on your desktop, ensure you have SDL2 installed. Compile `main.cpp` using your preferred C++ compiler or build system in VS Code. Once compiled, run the executable to emulate the device output on your computer.

---

## 📸 Demo Screenshots

1. **Speccy loading sequence** – Boots up in true ZX Spectrum style before switching gears.  
   ![Speccy loading](img/1.png)  
2. **Loading MacOS** – Watch MacOS 9 come to life on this tiny display.  
   ![MacOS loading](img/2.png)  
3. **Tunnel** – Simple pixel tunnel.  
   ![Tunnel](img/3.png)  
4. **DeanTheCoder QR code to GitHub site** – Scan to explore the project’s code and details.  
   ![QR code](img/4.png)  
5. **Retro fire effect** – Classic fire animation brings warmth to the display.  
   ![Retro fire](img/5.png)  
6. **Amiga Boing Ball** – Nostalgic bouncing ball animation inspired by the Amiga.  
   Inspired by the original [Amiga Boing Ball](https://en.wikipedia.org/wiki/Amiga).
   ![Boing Ball](img/6.png)  
7. **Spinning donut** – A mesmerizing 3D donut spins smoothly on screen.  
   Based on the [Donut Math article](https://www.a1k0n.net/2011/07/20/donut-math.html) from A1K0N.
   ![Donut](img/7.png)  
8. **Matrix rain** – Falling green code reminiscent of the Matrix movies.  
   Glyphs based on the work from [Rezmason's Matrix project](https://github.com/Rezmason/matrix).
   ![Matrix rain](img/8.png)  
9. **Conway's Game of Life** – Cellular automaton in action, evolving patterns on display.  
   See [Conway's Game of Life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life) for more details about the algorithm.
   ![Game of Life](img/9.png)  
10. **Kernel panic** – Occasionally throws a kernel panic to keep you guessing.  
   ![Kernel panic](img/10.png)  

---

## 🙏 Acknowledgements

- [A1K0N](https://www.a1k0n.net/2011/07/20/donut-math.html) for Donut Math.  
- [Rezmason](https://github.com/Rezmason/matrix) for Matrix glyphs.  
- [Wikipedia](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life) for Conway’s Game of Life reference.
