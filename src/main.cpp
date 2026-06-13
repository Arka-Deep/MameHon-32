#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <Bookerly9pt7b.h>

// --- MACROS (From above) ---
//E-Paper Display
#define EPD_PWR   7
#define EPD_SCK   12
#define EPD_MOSI  11
#define EPD_MISO  -1
#define EPD_CS    45
#define EPD_DC    46
#define EPD_RST   47
#define EPD_BUSY  48

//SD card
#define SD_PWR    42
#define SD_SCK    39 
#define SD_MOSI   40 
#define SD_MISO   13 
#define SD_CS     10 

//Rotary Switch
#define BTN_UP    6
#define BTN_DOWN  4
#define BTN_SAVE  5   // New Save / Cover Mode toggle button


//SPIClass EPD_SPI(FSPI); // Uses internal hardware SPI2 for E-ink
SPIClass SD_SPI(HSPI);  // Uses internal hardware SPI3 for SD Card

// Initialize Display
GxEPD2_BW<GxEPD2_290_T94_V2, GxEPD2_290_T94_V2::HEIGHT> display(GxEPD2_290_T94_V2(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));


// =========================================================================
// GLOBAL STATE & REFRESH CONFIGURATION
// =========================================================================
int fullRefreshInterval = 6;     // <-- CHANGE THIS to adjust how many pages run on partial refresh
int pagesSinceFullRefresh = 0;    // Counter tracking pages since the last full screen flash




// Global State Engine
uint32_t pageOffsets[500]; 
int currentPage = 0;
int maxDiscoveredPage = 0;
bool inCoverMode = false; 

// Allocate a 1-bit RAM graphic canvas buffer for the cover image (296x128 / 8 = 4736 bytes)
uint8_t coverImageBuffer[4736];

// Typographical Configurations
const GFXfont* readerFont = &Bookerly9pt7b;
const int leftMargin = 10;
const int rightMargin = 10;
const int lineSpacing = 6;





// =========================================================================
// BACKGROUND TEXT SCANNER (For layout calculation)
// =========================================================================
uint32_t scanNextPageOffset(File &bookFile, uint32_t startOffset) {
    bookFile.seek(startOffset);
    int maxWidth = display.width() - rightMargin;
    int maxHeight = display.height() - 5;
    
    int16_t bx, by; uint16_t bw, fontHeight;
    display.setFont(readerFont);
    display.getTextBounds("A", 0, 0, &bx, &by, &bw, &fontHeight);
    uint16_t spaceWidth = fontHeight / 3;

    int currentX = leftMargin;
    int currentY = fontHeight + lineSpacing;
    String word = "";

    while (bookFile.available()) {
        char c = bookFile.read();
        if (c == ' ' || c == '\n') {
            int16_t wx, wy; uint16_t wordWidth, wordHeight;
            display.getTextBounds(word.c_str(), 0, 0, &wx, &wy, &wordWidth, &wordHeight);

            if (currentX + wordWidth > maxWidth) {
                currentX = leftMargin;
                currentY += fontHeight + lineSpacing;
            }
            if (currentY > maxHeight) {
                return bookFile.position() - word.length() - 1;
            }
            if (c == '\n') {
                currentX = leftMargin;
                currentY += fontHeight + lineSpacing;
            } else {
                currentX += wordWidth + spaceWidth;
            }
            word = "";
        } else {
            word += c;
        }
    }
    return bookFile.position();
}

// =========================================================================
// TEXT PAGE RENDER ENGINE
// =========================================================================
void renderPage(int pageIndex) {
    
    File bookFile = SD.open("/book.txt", FILE_READ);
    if (!bookFile) return;

    uint32_t startOffset = pageOffsets[pageIndex];
    uint32_t nextOffset = 0;
    int maxWidth = display.width() - rightMargin;
    int maxHeight = display.height() - 5;
    
    int16_t bx, by; uint16_t bw, fontHeight;
    display.setFont(readerFont);
    display.getTextBounds("A", 0, 0, &bx, &by, &bw, &fontHeight);
    uint16_t spaceWidth = fontHeight / 3;

    


    // Determine whether to issue a clean full refresh or a fast partial update
    if (pagesSinceFullRefresh >= fullRefreshInterval) {
        Serial.println("Performing FULL refresh to clear screen ghosting...");
        display.setFullWindow();
        pagesSinceFullRefresh = 0; // Reset counter back to clean state
    } else {
        Serial.print("Performing fast PARTIAL refresh. Cycle count: "); 
        Serial.println(pagesSinceFullRefresh);
        display.setPartialWindow(0, 0, display.width(), display.height());
        pagesSinceFullRefresh++;
    }



    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        bookFile.seek(startOffset); 
        
        
        int currentX = leftMargin;
        int currentY = fontHeight + lineSpacing;
        String word = "";

        
        while (bookFile.available()) {
            char c = bookFile.read();
             
            
            if (c == ' ' || c == '\n') {
                int16_t wx, wy; uint16_t wordWidth, wordHeight;
                display.getTextBounds(word.c_str(), 0, 0, &wx, &wy, &wordWidth, &wordHeight);

                if (currentX + wordWidth > maxWidth) {
                    currentX = leftMargin;
                    currentY += fontHeight + lineSpacing;
                }
                if (currentY > maxHeight) {
                    
                    bookFile.seek(bookFile.position() - word.length() - 1);
                    
                    break; 
                }

                display.setCursor(currentX, currentY);
                display.print(word);

                if (c == '\n') {
                    currentX = leftMargin;
                    currentY += fontHeight + lineSpacing;
                } else {
                    currentX += wordWidth + spaceWidth;
                }
                word = "";
            } else {
                word += c;
            }
            
        }
         
    } while (display.nextPage());

    
    nextOffset = bookFile.position();
    bookFile.close();
    

    if (pageIndex == maxDiscoveredPage) {
        pageOffsets[pageIndex + 1] = nextOffset;
        maxDiscoveredPage++;
    }
}

// =========================================================================
// RAW BINARY Image(.BIN) LOADER
// =========================================================================
bool loadBINToBuffer(const char* filename) {
    
    File binFile = SD.open(filename, FILE_READ);
    if (!binFile) {
        Serial.println("Error: Missing cover.bin on SD card!");
        return false;
    }

    // A 296x128 1-bit raw array will always be exactly 4736 bytes
    if (binFile.size() != 4736) {
        Serial.println("Error: cover.bin file size is incorrect. Must be 4736 bytes.");
        binFile.close();
        return false;
    }

    // Dump the entire file directly into our 1-bit image buffer in one move
    binFile.read(coverImageBuffer, 4736);
    
    //inverting the image
    for (int i = 0; i < 4736; i++) {
      coverImageBuffer[i] = ~coverImageBuffer[i];
    }


    binFile.close();
    return true;
}

// =========================================================================
// BOOTSTRAP INITIALIZATION (COLD START LOGIC)
// =========================================================================
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n--- Cold Start: Initializing SD E-Reader ---");

    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_SAVE, INPUT_PULLUP);
    
    
    pinMode(SD_PWR, OUTPUT);
    digitalWrite(SD_PWR, HIGH);

    pinMode(EPD_PWR, OUTPUT);
    digitalWrite(EPD_PWR, HIGH);
    delay(150); 

    //Initializing E-Ink Display on its isolated SPI bus
    SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
    display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(0, true, 2, false);

    
    display.setRotation(1); 
    display.setTextColor(GxEPD_BLACK);

    //Initializing SD Card on its isolated SPI bus
    SD_SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, SD_SPI, 80000000)) { // Mounts SD directly onto SD_SPI at 20MHz
        Serial.println("SD Card initialization failed!");
        
        while(1); // Halt if SD card fails
    }

    Serial.println("System initialized with independent SPI buses.");

    // COLD START CHECK: Look for index record file on the SD card
    if (SD.exists("/save.txt")) {
        File saveFile = SD.open("/save.txt", FILE_READ);
        if (saveFile) {
            String storedVal = "";
            while (saveFile.available()) {
                storedVal += (char)saveFile.read();
            }
            currentPage = storedVal.toInt();
            saveFile.close();
            Serial.print("Resuming from SD Bookmark: Page "); Serial.println(currentPage);
        }
    } else {
        currentPage = 0;
        Serial.println("No save file detected. Starting at page 0.");
    }

    
    

    pageOffsets[0] = 0; 

    // Rebuild structural offset mapping up to current bookmark index
    if (currentPage > 0) {
        
        File bookFile = SD.open("/book.txt", FILE_READ);
        if (bookFile) {
            for (int i = 0; i < currentPage; i++) {
                pageOffsets[i + 1] = scanNextPageOffset(bookFile, pageOffsets[i]);
            }
            maxDiscoveredPage = currentPage;
            bookFile.close();
        }
    }

    // Force a crisp full flash on initial bootup to ensure memory is clear
    pagesSinceFullRefresh = fullRefreshInterval;

    // Render text to start reading immediately on boot
    renderPage(currentPage);
}

// =========================================================================
// RUNTIME NAVIGATIONAL CONTROLS
// =========================================================================
void loop() {
    // 1. ACTION: SAVE & TOGGLE COVER SCREEN LOCK MODE
    if (digitalRead(BTN_SAVE) == LOW) {
        delay(50); // Debounce
        if (digitalRead(BTN_SAVE) == LOW) {
            if (!inCoverMode) {
                Serial.println("Entering Cover Mode. Saving page index...");


                

                // Save current tracking index string onto SD card storage block
                
                SD.remove("/save.txt"); // Wipe out the old save record file
                File saveFile = SD.open("/save.txt", FILE_WRITE);
                if (saveFile) {
                    saveFile.print(currentPage);
                    saveFile.close();
                }

                // Load image from SD card into RAM buffer space
                if (loadBINToBuffer("/cover.bin")) {
                    
                    // FORCE FULL REFRESH WHEN ENTERING COVER MODE
                    display.setFullWindow();

                    // Flash the full screen array map onto the display matrix
                    display.firstPage();
                    do {
                        display.fillScreen(GxEPD_WHITE);
                        display.drawBitmap(0, 0, coverImageBuffer, 296, 128, GxEPD_BLACK);
                    } while (display.nextPage());
                    
                    inCoverMode = true;
                    Serial.println("Cover screen active. Safe to remove power.");
                }
            } else {
                Serial.println("Exiting Cover Mode. Resuming reading layout...");
                inCoverMode = false;

                // FORCE FULL REFRESH WHEN EXITING COVER MODE
                pagesSinceFullRefresh = fullRefreshInterval;


                renderPage(currentPage); // Redraw text data back onto display canvas
            }
            while (digitalRead(BTN_SAVE) == LOW); // Lock loop until physical release
        }
    }

    // Navigational directional steps are ONLY permitted while Cover Screen is inactive
    if (!inCoverMode) {
        // 2. ACTION: NEXT PAGE
        if (digitalRead(BTN_DOWN) == LOW) {
            delay(50); 
            if (digitalRead(BTN_DOWN) == LOW) {
                currentPage++;
                renderPage(currentPage);
                while (digitalRead(BTN_DOWN) == LOW); 
            }
        }

        // 3. ACTION: PREVIOUS PAGE
        if (digitalRead(BTN_UP) == LOW) {
            delay(50); 
            if (digitalRead(BTN_UP) == LOW) {
                if (currentPage > 0) {
                    currentPage--;
                    renderPage(currentPage);
                }
                while (digitalRead(BTN_UP) == LOW); 
            }
        }
    }
}