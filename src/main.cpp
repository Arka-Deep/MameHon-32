#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <Bookerly9pt7b.h>
#include <Bookerly6pt7b.h>

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

//LED
#define LED 41

//Menu and Exit buttons
#define BTN_MENU 2
#define BTN_EXIT 1


//SPIClass EPD_SPI(FSPI); // Uses internal hardware SPI2 for E-ink
SPIClass SD_SPI(HSPI);  // Uses internal hardware SPI3 for SD Card

// Initialize Display
GxEPD2_BW<GxEPD2_290_T94_V2, GxEPD2_290_T94_V2::HEIGHT> display(GxEPD2_290_T94_V2(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));


int fullRefreshInterval = 6;     
int pagesSinceFullRefresh = 0;   


bool darkMode = false;

uint16_t BG_COLOR = darkMode ? GxEPD_BLACK : GxEPD_WHITE;
uint16_t FG_COLOR = darkMode ? GxEPD_WHITE : GxEPD_BLACK;

uint8_t rotationVariable = 1;


uint32_t pageOffsets[500]; 
int currentPage = 0;
int maxDiscoveredPage = 0;
bool inCoverMode = false; 


uint8_t coverImageBuffer[4736];


const GFXfont* readerFont = &Bookerly9pt7b;
int leftMargin = 8;
int rightMargin = 6;
int lineSpacing = 6;


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


void renderPage(int pageIndex) {
    
    File bookFile = SD.open("/book.txt", FILE_READ);
    if (!bookFile) return;

    uint32_t startOffset = pageOffsets[pageIndex];
    uint32_t nextOffset = 0;
    int maxWidth = display.width() - rightMargin;
    int maxHeight = display.height() - 5;
    
    int16_t bx, by;
    uint16_t bw, fontHeight;
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
        display.fillScreen(BG_COLOR);
        display.setTextColor(FG_COLOR);
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


bool loadBINToBuffer(const char* filename) {
    
    File binFile = SD.open(filename, FILE_READ);
    if (!binFile) {
        Serial.println("Error: Missing cover.bin on SD card!");
        return false;
    }

    // A 296x128 1-bit array
    if (binFile.size() != 4736) {
        Serial.println("Error: cover.bin file size is incorrect. Must be 4736 bytes.");
        binFile.close();
        return false;
    }

  
    binFile.read(coverImageBuffer, 4736);
    



    binFile.close();
    return true;
}


void toggleRotation(){
    rotationVariable=(rotationVariable+1)%2;
    display.setRotation(rotationVariable);

    if(rotationVariable%2==0){
        leftMargin = 2;
        rightMargin = 2;
        lineSpacing = 4;
        readerFont = &Bookerly6pt7b;
    }else{
        leftMargin = 8;
        rightMargin = 6;
        lineSpacing = 6;

        readerFont = &Bookerly9pt7b;
    }
    pagesSinceFullRefresh = fullRefreshInterval;
    renderPage(currentPage);

}
void toggleDarkMode(){
    darkMode=!darkMode;  
    BG_COLOR = darkMode ? GxEPD_BLACK : GxEPD_WHITE;
    FG_COLOR = darkMode ? GxEPD_WHITE : GxEPD_BLACK;

    renderPage(currentPage);
}

void setup() {
    Serial.begin(115200);
    
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
            Serial.print("Resuming from SD Bookmark: Page ");
            Serial.println(currentPage);
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

    pagesSinceFullRefresh = fullRefreshInterval;
    renderPage(currentPage);
}


void loop() {
    // SAVE & TOGGLE COVER SCREEN LOCK MODE
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
                        display.fillScreen(GxEPD_BLACK);
                        display.drawBitmap(0, 0, coverImageBuffer, 296, 128, GxEPD_WHITE);
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

    
    if (!inCoverMode) {
        
        if (digitalRead(BTN_UP) == LOW) {
            delay(50); 
            if (digitalRead(BTN_UP) == LOW) {
                currentPage++;
                renderPage(currentPage);
                while (digitalRead(BTN_UP) == LOW); 
            }
        }

        
        if (digitalRead(BTN_DOWN) == LOW) {
            delay(50); 
            if (digitalRead(BTN_DOWN) == LOW) {
                if (currentPage > 0) {
                    currentPage--;
                    renderPage(currentPage);
                }
                while (digitalRead(BTN_DOWN) == LOW); 
            }
        }

        if(digitalRead(BTN_EXIT)==LOW){
            delay(50);
            if(digitalRead(BTN_EXIT)==LOW){
                
                toggleDarkMode();
                
            }
            while(digitalRead(BTN_EXIT)==LOW);
        }
        if(digitalRead(BTN_MENU)==LOW){
            delay(50);
            if(digitalRead(BTN_MENU)==LOW){
                
                
                toggleRotation();
            }
            while(digitalRead(BTN_MENU)==LOW);
        }
    }
}