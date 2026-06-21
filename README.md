# 📚 MameHon-32 E-Reader

[![Initial Release](https://img.shields.io/badge/Release-Initial_v1.0-blue.svg)](#)
[![Platform](https://img.shields.io/badge/Platform-ESP32-lightgrey.svg)](#)
[![Display](https://img.shields.io/badge/Display-2.9%22_E--Ink-black.svg)](#)

An Open Source E-Reader designed specifically for the [Elecrow CrowPanel ESP32 E-Paper HMI 2.9-inch Display](https://www.elecrow.com/crowpanel-esp32-2-9-e-paper-hmi-display-with-128-296-resolution-black-white-color-driven-by-spi-interface.html), though it can easily be modified for any other setup.

![E-Reader Demo](images/demo.gif)


---

## ✨ Features

* **Auto Resume:** Saves progress to the SD card (`save.txt`) when entering Cover Mode, ensuring you pick up right where you left off on the next boot. The saving is set to manual to minimise wear on the sd card(those are indeed quite expensive right now)
* **Dark Mode Toggle:** Toggle DarkMode with a single button.
 ![Dark Mode Demo](images/dark.gif)
* **Partial Refresh:** Alternates seamlessly between fast partial refreshes and full screen updates to eliminate e-ink ghosting. The interval is easily configurable in the code.
*  **Orientation Toggle:** Change To Landscape Or Vertical. Currently it only switches between two sides , for full rotation change ` %2 ` into ` %4 `
  
  ![Dark Mode Demo](images/orientation.gif)
* **Cover Lock Mode:** A single button press saves your progress and flashes a custom `.bin` cover image.


---

## 🛠️ Hardware & Pinouts

This firmware relies on the exact pinout of the CrowPanel 2.9". 

### ⚡ Power Enable Quirks
The CrowPanel features dedicated power control pins for both the E-Paper panel and the SD card slot. **These pins (`EPD_PWR` and `SD_PWR`) must be driven `HIGH` during setup**, or the peripherals will refuse to initialize.

### 📍 Pinout Map

| Component | Pin Name | ESP32 Pin | Notes / Function |
| :--- | :--- | :--- | :--- |
| **E-Paper** | EPD_PWR | 7 | **[Quirk]** Drives power to display. Must be `HIGH`. |
| | EPD_SCK | 12 | FSPI Clock |
| | EPD_MOSI | 11 | FSPI Data Out |
| | EPD_CS | 45 | Chip Select |
| | EPD_DC | 46 | Data/Command |
| | EPD_RST | 47 | Reset |
| | EPD_BUSY | 48 | Busy Signal |
| **SD Card** | SD_PWR | 42 | **[Quirk]** Drives power to SD slot. Must be `HIGH`. |
| | SD_SCK | 39 | HSPI Clock |
| | SD_MOSI | 40 | HSPI Data Out |
| | SD_MISO | 13 | HSPI Data In |
| | SD_CS | 10 | Chip Select |
| **Controls** | BTN_DOWN | 4 | Previous Page (Input Pullup) |
| | BTN_SAVE | 5 | Save Progress / Toggle Cover Mode (Input Pullup)|
| | BTN_UP | 6 | Next Page (Input Pullup) |
| | BTN_MENU | 2 | Toggle Landscape and Vertical (Input Pullup) |
| | BTN_EXIT | 1 | Toggle Dark mode (Input Pullup) |

---

## 📁 SD Card Setup

Format your MicroSD card to **FAT32**. You must place the following two files in the root directory:

1.  **`book.txt`**: Your reading material. Must be a standard plain text (`.txt`) file.
2.  **`cover.bin`**: The standby screen cover image.

### 📔 Preparing the Book 
The project in its current state only accepts plain text in utf-8 , use a tool like [Calibre](https://calibre-ebook.com/) to convert your E-Book library to plaintext. I might write my own EPUB parser later. Save it as **`book.txt`** at the root directory of the SD card

### 🖼️ Preparing the Cover Image (`cover.bin`)
To ensure rapid memory loading directly to the e-ink RAM buffer, the image must bypass processing and be formatted strictly as a raw binary file:

* **Dimensions:** Exactly **296 x 128 pixels**.
* **Format:** 1-bit Monochrome Raw Binary.
* **File Size:** It must be **exactly 4736 bytes**. The code will reject files of any other size to prevent buffer overflows.
* **Color Inversion:** The code inherently inverts the bits (`~coverImageBuffer[i]`) during load. 

**Conversion Tools:**
You can process your image using online converters like [image2cpp](https://javl.github.io/image2cpp/) (Set canvas to 296x128 -> Threshold -> Output as Plain Bytes) and save those bytes as a `.bin` file.
* *Settings:* `Output type: Binary (*.bin)` | `Scan Mode: Horizontal` | `BitsPixel: Monochrome (1-bit)` | `Max Width/Height: 296 x 128` | Uncheck "Include Head Data".

### 🔤 Changing the Font
The default firmware utilizes the `Bookerly9pt7b` font (mainly used in Kindles). If you wish to use a different font:
1.  Navigate to the **[Adafruit GFX Font Converter](https://rop.nl/truetype2gfx/)**.
2.  Upload your desired `.ttf` file and generate the `.h` file.
3.  Include it at the top of your sketch and assign it to the `readerFont` pointer.
4.  *Note:* You may need to tweak the `lineSpacing`, `leftMargin`, and `rightMargin` variables in the code depending on the ascenders/descenders of your new font.

---

## 🕹️ Controls & Operation

* **Next Page:** Press the `UP` button.
* **Previous Page:** Press the `DOWN` button.
* **Toggle Dark Mode:** Press the `EXIT` button.
* **Toggle Orientation:** Press the `MENU` button.
* **Toggle Cover Mode:** Press the `SAVE` button. 
    * *Activating* Cover Mode drops a bookmark in `save.txt`, pulls up the cover image, and locks the navigational buttons. **(Safe to power off).**
    * *Deactivating* Cover Mode parses your bookmark, runs a full-screen clear to eliminate artifacting, and renders your current page text. 

---

## 🚀 Porting to Other Hardware

While explicitly designed for the **Elecrow CrowPanel 2.9"**, this codebase can easily be adapted for generic ESP32 development boards hooked up to Waveshare or other GxEPD2-compatible e-paper displays. 

To adapt the code:
1.  Re-map the `EPD_*` and `SD_*` pin macros to match your dev board.
2.  Comment out or remove the `SD_PWR` and `EPD_PWR` `digitalWrite()` logic in `setup()` if your board manages peripheral power normally.
3.  Swap out the `GxEPD2_290_T94_V2` constructor for the specific panel model you are driving.

## 🖨️ 3D Printed Enclosure
stl files coming soon ...... once i save enough to buy a battery and design a case around it