[![Twitter URL](https://img.shields.io/twitter/url/https/twitter.com/deanthecoder.svg?style=social&label=Follow%20%40deanthecoder)](https://twitter.com/deanthecoder)

# 🧱 Brickintosh

A tiny Lego Macintosh powered by an ESP32-S3 + 1.69" LCD display.  
It like a Mac, boots as a ZX Spectrum, loads MacOS 8, and occasionally throws a BSOD just to keep you guessing.  
Because why settle for one retro system when you can mash them all together?

---

## ✨ Features

- **ZX Spectrum–style 'boot' and BASIC prompt**: `1982 Sinclair Research Ltd`.  
- **`LOAD "MACOS"`** sequence.  
- **MacOS 8 boot logo + progress bar**.  
- **Fake BSODs** (because nothing says “retro PC” like a STOP code).  
- Runs ASCII text-mode effects and animations after boot.  
- Powered by ESP32-S3 with Wi-Fi + USB-C, ready for OTA updates.  
- Fits neatly inside a Lego “mini Mac” case.  

---

## 🛠 Hardware

- [ESP32-S3 Dev Board with 1.69" IPS ST7789 LCD](https://thepihut.com/products/esp32-s3-development-board-with-1-69-lcd-display-240-x-280)  
- Optional: LiPo battery (JST-PH) for portable use (charging handled onboard).  
- Lego bricks (to taste) for the iconic case.  

---

## 📦 Software

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) for LCD driving.  
- Arduino framework.  
- RGB565-encoded visuals.  
- Optional OTA update support for wireless flashing.  

---