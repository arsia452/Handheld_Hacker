// -------------------- Libraries -------------------- //
#include <Wire.h>
#include <Adafruit_GFX.h>     
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <AiEsp32RotaryEncoder.h>
#include <SoftwareSerial.h>

// -------------------- Keyboard Start --------------------//
// Relevant Definitions
// Operating Voltage: 5V = Red
// BLK = GND
// YEL = SDA = GPIO8 by default
// WHI = SCL = GPIO9 by default
const int CARDKB_ADDR = 0x5F; // Default I2C address for CardKB

// Just here to prevent annoyance; doesn't do anything.
#define TFT_CS 10
#define TFT_RST 6
#define TFT_DC  7
#define TFT_MOSI  11  // SDA // HW MOSI on ESP32-S3
#define TFT_SCLK  12  // SCL // HW SCLK on ESP32-S3
#define TFT_BL  4    // LED back-light

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

void KB_startup()
{
  Wire.begin(); // Initialize I2C (default pins GPIO 8 & 9)
  Serial.println("CardKB Initialized. Type something!");
}

void KB_run() 
{
  // Request 1 byte from the CardKB
  Wire.requestFrom(CARDKB_ADDR, 1);
  
  // Check if the device is present
  if (Wire.available()) 
  {
    char c = Wire.read();
    
    // If a key is pressed
    if (c != 0)
    {
      // Keep track of cursor location
      int x_pos = tft.getCursorX();
      int y_pos = tft.getCursorY();
      
      switch (c) 
      {
        // TODO: Implement backspace
        // Case: Delete key (0x08)
        case 8:
          if (x_pos > 0)
          {
            tft.setCursor(x_pos-12, y_pos);
            tft.print("\b");
            tft.setCursor(x_pos, y_pos);
          }
          break;

        // Case: Tab key (0x09)
        case 9:
          tft.print("Tab");
          break;
        
        // Case: Enter key (0x0D)
        case 13:
          tft.print("\n");
          break;

        // Case: Esc (0x1B)
        case 27: 
          tft.print("Esc");
          break;

        // If left arrow is pressed; create grid UI
        case 180: 
          Serial.println("\nLeft: Grid UI");
          TFT_clear();
          gridUI();
          break;
        
        // If up arrow key is pressed; clear screen
        case 181:
          Serial.println("\nUp: Clear Screen");
          TFT_clear();
          break;
        
        // If down arrow is pressed; create test screen
        case 182:
          Serial.println("\nDown: Test Screen");
          TFT_clear();
          tftPrintTest();
          break;
        
        // If right arrow is pressed; create selection UI
        case 183:
          Serial.println("\nRight: Selection UI");
          TFT_clear();
          selectionUI();
          break;
        
        // Otherwise, print character to screen
        default:
          Serial.print(c, HEX);
          tft.print(c);
          break;
      }
    }
  }
  delay(10); // Small delay to prevent I2C flooding
}

// -------------------- Keyboard End --------------------//
