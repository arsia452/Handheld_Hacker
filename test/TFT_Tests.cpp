// Libraries
#include <Adafruit_GFX.h>     
#include <Adafruit_ST7789.h>


// -------------------- TFT Start --------------------//
// Relevant Definitions
// Operating Voltage: 3.3V 

#define TFT_CS 10
#define TFT_RST 6
#define TFT_DC  7
#define TFT_MOSI  11  // SDA // HW MOSI on ESP32-S3
#define TFT_SCLK  12  // SCL // HW SCLK on ESP32-S3
#define TFT_BL  4    // LED back-light

float p = 3.1415926; // Establish pi for tests

// Screen: ST7789, 320 x 170, Green Tab
// TODO: Implement Scaling
int TFT_Width = 320; // Change to width of display for scaling to work
int TFT_Height = 170; // Change to height of display for scaling to work
int TFT_Center_X = TFT_Width/2;
int TFT_Center_Y = TFT_Height/2;

// Create screen object
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);


// Method to start the TFT screen
void TFT_init()
{
  pinMode(TFT_BL, OUTPUT);                  // Turn backlight pin to output
  digitalWrite(TFT_BL, HIGH);               // Turn backlight on

  // For a 320 x 170 (which this is) screen, the center is located at 160, 85
  tft.init(TFT_Height, TFT_Width, SPI_MODE3);            // Initialize the TFT
  
  tft.setRotation(1);                       // Orientation; 1 = Horizontal
  
  tft.fillScreen(ST77XX_BLACK);             // Background color
  
  tft.setTextColor(ST77XX_WHITE);           // Defines text color
  
  tft.setTextSize(2);                       // Define text size
  
  tft.setCursor(0, 0);                      // Cursor location
  
  tft.println("Screen on!");                // Print words at corner
  
  tft.drawCircle(TFT_Center_X, TFT_Center_Y, 5, ST77XX_GREEN); // Print a green circle at the center of display
}

// UI Option 1:
// Draws 8 sqauares in a grid pattern as a UI testing grounds
// Just draws 8 rectangles in various colors with test text; debugging/prototyping only
void gridUI() {

  tft.drawRect(20, 15, 50, 50, ST77XX_RED);
  tft.setCursor(28, 70);
  tft.println("Icon 1");

  tft.drawRect(95, 15, 50, 50, ST77XX_GREEN);
  tft.setCursor(103, 70);
  tft.println("Icon 2");

  tft.drawRect(170, 15, 50, 50, ST77XX_BLUE);
  tft.setCursor(178, 70);
  tft.println("Icon 3");

  tft.drawRect(245, 15, 50, 50, ST77XX_YELLOW);
  tft.setCursor(253, 70);
  tft.println("Icon 4");

  tft.drawRect(20, 90, 50, 50, ST77XX_ORANGE);
  tft.setCursor(28, 145);
  tft.println("Icon 5");

  tft.drawRect(95, 90, 50, 50, ST77XX_MAGENTA);
  tft.setCursor(103, 145);
  tft.println("Icon 6");

  tft.drawRect(170, 90, 50, 50, ST77XX_CYAN);
  tft.setCursor(178, 145);
  tft.println("Icon 7");

  tft.drawRect(245, 90, 50, 50, ST77XX_WHITE);
  tft.setCursor(253, 145);
  tft.println("Icon 8");
}

// UI Option 2: 
// Creates a selection UI with arrows and a primary selected icon
void selectionUI() {
  tft.drawRect(110, 35, 100, 100, ST77XX_CYAN);
  tft.setCursor(110, 145);
  tft.setTextSize(2);
  tft.println("Selected");

  tft.drawRect(20, 52, 65, 65, ST77XX_GREEN);
  tft.setCursor(20, 145);
  tft.println("<<");

  tft.drawRect(235, 52, 65, 65, ST77XX_GREEN);
  tft.setCursor(275, 145);
  tft.println(">>");
}

// Extended proof of life
void tftPrintTest() {
  Serial.println("Entered print test!");
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 30);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(1);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(3);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_BLUE);
  tft.setTextSize(4);
  tft.print(1234.567);
  delay(1500);
  tft.setCursor(0, 0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(0);
  tft.println("Hello World!");
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN);
  tft.print(p, 6);
  tft.println(" Want pi?");
  tft.println(" ");
  tft.print(8675309, HEX); // print 8,675,309 out in HEX!
  tft.println(" Print HEX!");
  tft.println(" ");
  tft.setTextColor(ST77XX_WHITE);
  tft.println("Sketch has been");
  tft.println("running for: ");
  tft.setTextColor(ST77XX_MAGENTA);
  tft.print(millis() / 1000);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(" seconds.");
}
