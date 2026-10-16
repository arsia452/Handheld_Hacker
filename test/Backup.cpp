// -------------------- Libraries -------------------- //
#include <FreeRTOS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>     
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <AiEsp32RotaryEncoder.h>
#include <SoftwareSerial.h>
#include <HardwareSerial.h>
#include <Adafruit_PN532.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

// -------------------- Definitions -------------------- //
// LED Test Task Pin
#define LED_PIN 17

// SPI Bus Pins
#define SPI_BUS_MOSI  5  // MOSI/SDA; 11 is HW MOSI on ESP32-S3-Devkit-C1
#define SPI_BUS_MISO  6   // MISO; 10 is default HW MISO
#define SPI_BUS_SCLK  7  // SCL; 12 HW SCLK on ESP32-S3-Devkit-C1

// I2C Bus Pins
#define I2C_BUS_SDA  13
#define I2C_BUS_SCL  14

// TFT ST7789
#define TFT_CS   8
#define TFT_RST  21
#define TFT_DC   38
#define TFT_BL   39  // LED backlight

// Rotary Encoder
#define RENC_CLK   1
#define RENC_DT    2
#define RENC_SW    40
#define RENC_VCC  -1
#define RENC_step  4

// RDM6300
#define RDM_RX_PIN  4   // Safe GPIO; ESP32 PIN 4 <-> RDM TX PIN
#define RDM_TX_PIN -1  // No actual connection here; just for func def

// CC1101
#define CC1101_CS    10 // Chip select for SPI bus
#define CC1101_GDO0  11 // GDO0 pin

// Define a queue
QueueHandle_t inputQueue;

// -------------------- Non-task Functions -------------------- //

// -=-=-=-= InputEvent Functions =-=-=-=- //
enum InputType {INPUT_ENCODER_CW, INPUT_ENCODER_CCW, INPUT_ENCODER_BTN, INPUT_KEY, INPUT_L_BTN, INPUT_R_BTN};

struct InputEvent
{
  InputType type;
  char key; // only used for INPUT_KEY, holds the actual character from CardKB
};

// -=-=-=-= Scene Definition =-=-=-=- //

enum SceneType {SCENE_MENU, SCENE_TEST1, SCENE_TEST2, 
  SCENE_RFID, SCENE_NFC, SCENE_RF, SCENE_IR, SCENE_SETTINGS, 
  SCENE_WIFI, SCENE_BT, SCENE_NOTES, SCENE_FILES};

SceneType currentScene = SCENE_MENU;

struct Scene {
  const char* name;
  SceneType type;
};

Scene scenes[] = {
  {"Menu",      SCENE_MENU},
  {"Test 1",     SCENE_TEST1},
  {"Test 2",     SCENE_TEST2},
  {"RFID",      SCENE_RFID},
  {"NFC",       SCENE_NFC},
  {"Sub-GHz",   SCENE_RF},
  {"IR",        SCENE_IR},
  {"WiFi",      SCENE_WIFI},
  {"Bluetooth", SCENE_BT},
  {"Settings",  SCENE_SETTINGS},
  {"Notes",     SCENE_NOTES},
  {"Files",     SCENE_FILES}
};


const int SCENE_COUNT = sizeof(scenes) / sizeof(scenes[0]);
int selectedIndex = 0; // current menu position


// -=-=-=-= TFT Functions =-=-=-=- //

// Create TFT Object and define relevant parameters
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, SPI_BUS_MOSI, SPI_BUS_SCLK, TFT_RST);
int TFT_Width = 320; // Change to width of display for scaling to work
int TFT_Height = 170; // Change to height of display for scaling to work
int TFT_Center_X = TFT_Width/2;
int TFT_Center_Y = TFT_Height/2;

// Function to clear the screen and reset cursor to top right
void TFT_clear()
{
  tft.setCursor(0, 0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
}

// Function to draw status bar
void TFT_drawStatusBar()
{
  tft.setTextSize(2);
  tft.setCursor(0, 2);
  tft.println("Time Here");
  tft.setCursor(170, 2);
  tft.println("bat_ico xx%");
  tft.drawLine(0, 19, 320, 19, ST77XX_WHITE);
  tft.drawLine(0, 20, 320, 20, ST77XX_WHITE);
}

// Grid layout constants
const int GRID_COLS = 4;
const int GRID_START_X = 20;
const int GRID_START_Y = 30;
const int GRID_CELL_W = 50;
const int GRID_CELL_H = 50;
const int GRID_GAP_X = 75;  // distance between cell origins horizontally
const int GRID_GAP_Y = 75;  // distance between cell origins vertically

void drawGrid()
{
  tft.setTextSize(1);

  for (int i = 0; i < SCENE_COUNT; i++)
  {
    int col = i % GRID_COLS;
    int row = i / GRID_COLS;

    int x = GRID_START_X + col * GRID_GAP_X;
    int y = GRID_START_Y + row * GRID_GAP_Y;

    // Highlight selected
    uint16_t color = (i == selectedIndex) ? ST77XX_WHITE : ST77XX_RED;

    tft.drawRect(x, y, GRID_CELL_W, GRID_CELL_H, color);
    tft.setCursor(x + 8, y + GRID_CELL_H + 5);
    tft.println(scenes[i].name);
  }
}



// -=-=-=-= Rotary Encoder Functions =-=-=-=- //
// Create rotary encoder object
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(RENC_DT, RENC_CLK, RENC_SW, RENC_VCC, RENC_step);
void rotary_onButtonClick()
{
    static unsigned long lastTimePressed = 0; // Soft debouncing
    if (millis() - lastTimePressed < 500)
    {
      // Send data to TFT
      TFT_clear();
      return;
    }
    lastTimePressed = millis();
    Serial.print("Button pressed!\n");
}

void IRAM_ATTR readEncoderISR()
{
    rotaryEncoder.readEncoder_ISR();
}

void rEnc_init()
{
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);

  bool rollover = true;
  rotaryEncoder.setBoundaries(0, 1000, rollover);

  rotaryEncoder.disableAcceleration();
}



// -=-=-=-= RDM6300 Functions =-=-=-=- //
// Create hardware serial object for RDM6300
HardwareSerial rdmSerial(2);  // UART2
void RDM_init()
{
  rdmSerial.begin(9600, SERIAL_8N1, RDM_RX_PIN, RDM_TX_PIN);
  Serial.println("RDM6300 ready.");
}



// -=-=-=-= PN532 Functions =-=-=-=- //
// Create PN532 object on I2C bus
const int PN532_ADDR = 0x24; // Default I2C address for PN532
Adafruit_PN532 nfc(-1, -1); // I2C mode. Reset and IRQ pins are -1.
void PN_init()
{
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata)
  {
    Serial.println("PN532 not found!");
    while (1) delay(10);
  }
  Serial.println("PN532 Initialized. Scan something!");
  nfc.SAMConfig();
}



// -=-=-=-= CC1101 Functions =-=-=-=- //
// Create CC1101 object on SPI bus
void CC_init()
{
  ELECHOUSE_cc1101.setSpiPin(SPI_BUS_SCLK, SPI_BUS_MISO, SPI_BUS_MOSI, CC1101_CS);

  ELECHOUSE_cc1101.Init();

  if (ELECHOUSE_cc1101.getCC1101())
    Serial.println("CC1101 connected!");
  else
    Serial.println("CC1101 not found!");

  ELECHOUSE_cc1101.setMHZ(313.8); // Set frequency
  ELECHOUSE_cc1101.setRxBW(812.50); // Bandwidth to as wide as it will go
  Serial.println("CC1101 initialized.");
}



// -=-=-=-= Keyboard Functions =-=-=-=- //
const int CARDKB_ADDR = 0x5F; // Default I2C address for CardKB
void CardKB_init()
{
  Serial.println("CardKB Initialized. Type something!");
  // TODO: Would like to add a check to see if the keyboard is here.
}



// -=-=-=-= Bus Functions =-=-=-=- //
void I2C_init()
{
  pinMode(I2C_BUS_SDA, INPUT_PULLUP); // Needs 4.7kΩ pullup to work (, INPUT_PULLUP)
  pinMode(I2C_BUS_SCL, INPUT_PULLUP); // Needs 4.7kΩ pullup to work (, , INPUT_PULLUP)
  Wire.begin(I2C_BUS_SDA, I2C_BUS_SCL); // Initialize I2C (default pins GPIO 8 & 9) 
}



// --------------- Tasks --------------- //

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=
LED Task:
Just a proof of life task that is 
always operating in the background. 
Voltage: R1 330Ω + 3.3V
-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

// LED Task handle
TaskHandle_t ledTaskHandle = NULL;

void ledTask(void *pvParameters) 
{
  // Initialize the onboard LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Run once to retrieve core; debugging
  int core = xPortGetCoreID();

  for (;;) {

    // Toggle the LED
    digitalWrite(LED_PIN, HIGH);
    // Serial.printf("LED ON - Running on core %d\n", core);
    vTaskDelay(pdMS_TO_TICKS(1000));
    digitalWrite(LED_PIN, LOW);
    // Serial.printf("LED OFF - Running on core %d\n", core);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
  }
}


/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=
Rotary Encoder Task:
Handles all rotary encoder inputs.
Voltage: 5V
-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

// Rotary Encoder Task Handle
TaskHandle_t rEncHandle = NULL;

void rEncTask(void *pvParameters)
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID();
  long lastValue;

  for (;;)
  {
    if (rotaryEncoder.encoderChanged())
    {
      InputEvent evt;
      long value = rotaryEncoder.readEncoder();
      evt.type = (rotaryEncoder.readEncoder() > lastValue) ? INPUT_ENCODER_CW: INPUT_ENCODER_CCW;
      lastValue = value;
      xQueueSend(inputQueue, &evt, 0);
    }
    if (rotaryEncoder.isEncoderButtonClicked())
    {
      // rotary_onButtonClick();
      InputEvent evt;
      evt.type = INPUT_ENCODER_BTN;
      xQueueSend(inputQueue, &evt, 0);
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // 100Hz polling; poll every 10ms
  }

}



/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=
Keyboard Task:
Handles inputs from cardKB over I2C.
Voltage: 5V
-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

// CardKB Task Handle
TaskHandle_t kbHandle = NULL;

void CardKBTask(void *pvParameters)
{
  for (;;)
  {
  Wire.requestFrom(CARDKB_ADDR, 1);
  if (Wire.available())
    {
    char c = Wire.read();
    if (c != 0)
      {
        InputEvent evt;
        evt.key = c;
        evt.type = INPUT_KEY;
        xQueueSend(inputQueue, &evt, 0);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // 100Hz polling is perfectly adequate
  }
}



/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=
RDM6300 Task:
Handles all RDM6300 inputs and associated UI outputs.
Voltage: 5V
-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

// RDM6300 Task Handle
TaskHandle_t rdmHandle = NULL;

void RDMTask(void *pvParameters)
{

  RDM_init(); // Run once; initialize RDM6300
  
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID();

  for (;;)
  {
    while (rdmSerial.available()) {
      // TODO: Normalize into 14-byte string.
      byte b = rdmSerial.read();
      Serial.printf("RDM6300 got byte: 0x%02X - Running on core %d\n", b, core);
      delay(10); // Prevent flooding
      // TODO: Add check; if it sees 0xE0 on first boot, 
      // that means it found the RDM6300. 
      // If not, it didn't find it. Tell the user.

      // TODO: Send data to TFT

    }

    vTaskDelay(pdMS_TO_TICKS(10)); // 100Hz polling; poll every 10ms
  }
}



/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=
PN532 Task:
Handles all PN532 inputs and associated UI outputs.
Voltage: 3.3V (claims it can handle up to 5.5V, but I don't want to FAFO here.)
-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

// PN532 Task Handle
TaskHandle_t pnHandle = NULL;

void PNTask(void *pvParameters)
{

  PN_init(); // Run once; initialize PN532

   // Run once to retrieve core; debugging
  int core = xPortGetCoreID();

  for (;;)
  {
    uint8_t uid[7];
    uint8_t uidLength;

    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength))
    {
      Serial.print("Found tag with UID: ");
      for (uint8_t i = 0; i < uidLength; i++)
      {
        Serial.printf("0x%02X ", uid[i]);
      }
      Serial.println();
    }
  
    vTaskDelay(pdMS_TO_TICKS(10)); // 100Hz polling; poll every 10ms
  }
}



/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=
PN532 Task:
Handles all PN532 inputs and associated UI outputs.
Voltage: 3.3V (claims it can handle up to 5.5V, but I don't want to FAFO here.)
-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

// CC1101 Task Handle
TaskHandle_t ccHandle = NULL;

void CCTask(void *pvParameters)
{
  CC_init(); // Run once; initialize CC1101

   // Run once to retrieve core; debugging
  int core = xPortGetCoreID();

  float freqs[] = {310.0, 311.0, 312.0, 313.0, 313.8, 314.0, 315.0, 316.0, 317.0};
  int freqCount = sizeof(freqs) / sizeof(freqs[0]);

  for (;;)
  {
    for (int i = 0; i < freqCount; i++)
    {
      ELECHOUSE_cc1101.setMHZ(freqs[i]);
      vTaskDelay(pdMS_TO_TICKS(100));
      int rssi = ELECHOUSE_cc1101.getRssi();
      Serial.printf("%.1f MHz: %d dBm\n", freqs[i], rssi);
    }
    Serial.println("---");
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  // Serial.printf("Freq: %.2f MHz | RSSI: %d dBm | Core: %d\n", freq, rssi, core);

}

// -=-=-=-= Scene Manager Functions =-=-=-=- // 

// Global Vars
bool firstDraw = true; // Flag: first time drawing menu
bool menuRedraw = false; // Flag: menu needs redrawing

// Debugging drawing 1
void drawTest1()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Framerate capping: 100ms = 10 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  // If scene is new/refreshed
  if (firstDraw)
  {
    TFT_clear();
    tft.setTextSize(2);
    firstDraw = false;
  }

  // Update screen info:
  // int randNum = random(0, 1000);
  // tft.print(randNum);
  // tft.drawCircle(TFT_Center_X, TFT_Center_Y, 5, ST77XX_RED);
  // tft.println("\nTest 1 works!");
}

// Debugging drawing 2
void drawTest2()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Framerate capping: 100ms = 10 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  // If scene is new/refreshed
  if (firstDraw)
  {
    TFT_clear();
    tft.setTextSize(2);
    tft.print("Dev Environment\n");
    tft.drawLine(0, 18, 320, 18, ST77XX_RED);
    tft.setCursor(0, 34);
    firstDraw = false;
  }

  // Update screen info:
  
  // int randNum = random(0, 1000);
  // tft.print(randNum);
  // tft.drawCircle(TFT_Center_X, TFT_Center_Y, 5, ST77XX_GREEN);
  // tft.println("\nTest 2 works!");
}

// Draw a menu
void drawMenu()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Framerate capping: 
  // 1000 / ms = fps cap
  // 100 = ms -> 10 fps cap
  // 33ms = ms -> 30 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 33) return; 
  lastDraw = millis();

  // If scene is new
  if (firstDraw)
  {
    TFT_clear();
    TFT_drawStatusBar();
    menuRedraw = true;
    firstDraw = false;
  }

  // If scene is refreshed
  if (menuRedraw)
  {
    drawGrid();
    menuRedraw = false;
  }

  // May need other flags later; same format
}

// Draw RFID (RDM6300) Screen
void drawRFIDScreen()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID();

  // Framerate capping: 100ms = 10 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  // If scene is new/refreshed
  if (firstDraw)
  {
    TFT_clear();
    tft.setTextSize(2);
    tft.print("RFID: In development.\n");
    firstDraw = false;
  }

  
  // Update screen info:

}

// Draw NFC (PN532) Screen
void drawNFCScreen()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Framerate capping: 100ms = 10 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  // If scene is new/refreshed
  if (firstDraw)
  {
    TFT_clear();
    tft.setTextSize(2);
    tft.print("NFC: In development.\n");
    firstDraw = false;
  }

  // Update screen info:

}

// Draw RF (CC1101) Screen
void drawRFScreen()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Framerate capping: 100ms = 10 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  // If scene is new/refreshed
  if (firstDraw)
  {
    TFT_clear();
    tft.setTextSize(2);
    tft.print("Sub-GHz: In development.\n");
    firstDraw = false;
  }

  // Update screen info:
}

// Draw IR (TSOP1838/TX) Screen
void drawIRScreen()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Framerate capping: 100ms = 10 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  // If scene is new/refreshed
  if (firstDraw)
  {
    TFT_clear();
    tft.setTextSize(2);
    tft.print("IR TX/RX: In development.\n");
    firstDraw = false;
  }

  // Update screen info:
}

// Draw Settings Screen
void drawSettingsScreen()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Framerate capping: 100ms = 10 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  // If scene is new/refreshed
  if (firstDraw)
  {
    TFT_clear();
    tft.setTextSize(2);
    tft.print("Settings: In development.\n");
    firstDraw = false;
  }

  // Update screen info:
}

// Draw WiFi Screen
void drawWifiScreen()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Framerate capping: 100ms = 10 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  // If scene is new/refreshed
  if (firstDraw)
  {
    TFT_clear();
    tft.setTextSize(2);
    tft.print("Wi-Fi: In development.\n");
    firstDraw = false;
  }

  // Update screen info:
}

// Draw Blueooth Screen
void drawBTScreen()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Framerate capping: 100ms = 10 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  // If scene is new/refreshed
  if (firstDraw)
  {
    TFT_clear();
    tft.setTextSize(2);
    tft.print("BT Classic + BLE5.0: In development.\n");
    firstDraw = false;
  }

  // Update screen info:
}

// Draw Notes Screen
void drawNotesScreen()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Framerate capping: 100ms = 10 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  // If scene is new/refreshed
  if (firstDraw)
  {
    TFT_clear();
    tft.setTextSize(2);
    tft.print("Notes: In development.\n");
    firstDraw = false;
  }

  // Update screen info:
}

// Draw Files Screen
void drawFilesScreen()
{
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Framerate capping: 100ms = 10 fps cap
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 100) return;
  lastDraw = millis();

  // If scene is new/refreshed
  if (firstDraw)
  {
    TFT_clear();
    tft.setTextSize(2);
    tft.print("File Browser: In development.\n");
    firstDraw = false;
  }

  // Update screen info:
}

// Draw a scene by passing scene as parameter
void sceneManager_draw(SceneType scene)
{
  switch (scene)
  {
    case SCENE_TEST1: drawTest1(); break;
    case SCENE_TEST2: drawTest2(); break;
    case SCENE_MENU: drawMenu(); break;
    case SCENE_RFID: drawRFIDScreen(); break;
    case SCENE_NFC: drawNFCScreen(); break;
    case SCENE_RF: drawRFScreen(); break;
    case SCENE_IR: drawIRScreen(); break;
    case SCENE_SETTINGS: drawSettingsScreen(); break;
    case SCENE_WIFI: drawWifiScreen(); break;
    case SCENE_BT: drawBTScreen(); break;
    case SCENE_NOTES: drawNotesScreen(); break;
    case SCENE_FILES: drawFilesScreen(); break;
    // may add more here
  }
}

// Scene Manager Enter
void sceneManager_enter(SceneType scene)
{
  // Spin up task that pertains to scene
  currentScene = scene;

  // Screen needs refresh
  firstDraw = true;

  switch (scene)
  {
    case SCENE_RFID:
      xTaskCreatePinnedToCore(
        RDMTask,      // Function
        "RDM6300",    // Name
        4096,         // Stack size (bytes)
        NULL,         // Parameters
        1,            // Priority
        &rdmHandle,   // Handle
        1             // Core
      );
      Serial.println("Successfully started RDM6300 Task.");
      break;

      case SCENE_RF:
        xTaskCreatePinnedToCore(
        CCTask,       // Function
        "CC1101",     // Name
        4096,         // Stack size (bytes)
        NULL,         // Parameters
        1,            // Priority
        &ccHandle,    // Handle
        1             // Core
      );
      Serial.println("Successfully started CC1101 Task.");
      break;

    default:
      break;
  }

}

// Scene Manager Exit
void sceneManager_exit()
{
  Serial.println(currentScene);

  switch (currentScene)
  {
    case SCENE_RFID:
      Serial.println("Exiting RFID.");
      vTaskDelete(rdmHandle);
      rdmHandle = NULL;
      break;

    case SCENE_RF:
      Serial.println("Exiting Radio.");
      vTaskDelete(ccHandle);
      ccHandle = NULL;
      break;

    default:
      break;
  }


  currentScene = SCENE_MENU;
  Serial.println(currentScene);

  firstDraw = true;
  selectedIndex = 0;
  sceneManager_draw(currentScene);

}

// Handle input on debugging screen 1
void test1HandleInput(InputEvent evt)
{
  switch (evt.type)
      {
        case INPUT_ENCODER_CW:
          tft.println("CW");
          Serial.printf("Rotary Direction: CW \n");
          break;
        
        case INPUT_ENCODER_CCW:
          tft.println("CCW");
          Serial.printf("Rotary Direction: CCW \n");
          break;
        
        case INPUT_ENCODER_BTN:
          currentScene = SCENE_TEST2;
          sceneManager_draw(currentScene);
          Serial.printf("Encoder button pressed.\n");
          break;

        case INPUT_KEY:
          switch (evt.key)
          {
            case 0x1B: // Escape
              sceneManager_exit();
              break;
            
            default:
              tft.print(evt.key);
              break;
          }
          break; // Prevent fallthrough

        case INPUT_L_BTN:
          break;
        
        case INPUT_R_BTN:
          break;
      }
}

// Handle input on debugging screen 2
void test2HandleInput(InputEvent evt)
{
  switch (evt.type)
  {
    case INPUT_ENCODER_CW:
      tft.println("CW");
      Serial.printf("Rotary Direction: CW \n");
      break;
    
    case INPUT_ENCODER_CCW:
      tft.println("CCW");
      Serial.printf("Rotary Direction: CCW \n");
      break;
    
    case INPUT_ENCODER_BTN:
      currentScene = SCENE_TEST1;
      sceneManager_draw(currentScene);
      Serial.printf("Encoder button pressed.\n");
      break;

    case INPUT_KEY:
      switch (evt.key)
      {
        case 0x1B: // Escape
          sceneManager_exit();
          break;

        default:
          tft.print(evt.key);
          break;
      }
      break; // Prevent fallthrough

    case INPUT_L_BTN:
      break;
    
    case INPUT_R_BTN:
      break;
  }
}

// Handle input on menu screen
void menuHandleInput(InputEvent evt)
{
  switch (evt.type)
  {
    case INPUT_ENCODER_CW:
      // increment selection ++;
      selectedIndex = (selectedIndex + 1) % SCENE_COUNT; // Wraparound
      Serial.printf("Selected: %s - CW\n", scenes[selectedIndex].name); 
      menuRedraw = true;
      break;
    
    case INPUT_ENCODER_CCW:
      // decrement selection --;
      selectedIndex = (selectedIndex - 1 + SCENE_COUNT) % SCENE_COUNT; // Wraparound
      Serial.printf("Selected: %s - CCW\n", scenes[selectedIndex].name);
      menuRedraw = true;
      break;
    
    case INPUT_ENCODER_BTN:
      sceneManager_enter(scenes[selectedIndex].type);
      Serial.printf("Encoder button pressed. %s entered.\n", scenes[selectedIndex].name);
      break;

    case INPUT_KEY:
      switch (evt.key)
      {
        case 0x1B: // Escape
          sceneManager_exit();
          break;

        case 0x31: // 1
          currentScene = SCENE_TEST1;
          sceneManager_draw(currentScene);
          break;
        
        case 0x32: // 2
          currentScene = SCENE_TEST2;
          sceneManager_draw(currentScene);
          break;

        case 0xB4: // Left
          selectedIndex = (selectedIndex - 1 + SCENE_COUNT) % SCENE_COUNT; // Wraparound
          menuRedraw = true;
          break;
        
        case 0xB5: // Up
          selectedIndex = (selectedIndex - 4 + SCENE_COUNT) % SCENE_COUNT; // Wraparound
          menuRedraw = true;
          break;
        
        case 0xB6: // Down
          selectedIndex = (selectedIndex + 4) % SCENE_COUNT; // Wraparound
          menuRedraw = true;
          break;

        case 0xB7: // Right
          selectedIndex = (selectedIndex + 1) % SCENE_COUNT; // Wraparound
          menuRedraw = true;
          break;

        default:
          break;
      }
      break;

    case INPUT_L_BTN:
      break;
    
    case INPUT_R_BTN:
      break;
  }
}

// Handle input on RFID screen
void rfidHandleInput(InputEvent evt)
{
  switch (evt.type)
  {
    case INPUT_ENCODER_CW:
      break;
    
    case INPUT_ENCODER_CCW:
      break;
    
    case INPUT_ENCODER_BTN:
      break;

    case INPUT_KEY:
      switch (evt.key)
      {
        case 0x1B: // Escape
          sceneManager_exit();
          break;

        default:
          break;
      }
      break;

    case INPUT_L_BTN:
      break;
    
    case INPUT_R_BTN:
      break;
  }
}

// Handle input on NFC screen
void nfcHandleInput(InputEvent evt)
{
  switch (evt.type)
  {
    case INPUT_ENCODER_CW:
      break;
    
    case INPUT_ENCODER_CCW:
      break;
    
    case INPUT_ENCODER_BTN:
      break;

    case INPUT_KEY:
      switch (evt.key)
      {
        case 0x1B: // Escape
          sceneManager_exit();
          break;

        default:
          break;
      }
      break;

    case INPUT_L_BTN:
      break;
    
    case INPUT_R_BTN:
      break;
  }
}

// Handle input on RF screen
void rfHandleInput(InputEvent evt)
{
  switch (evt.type)
  {
    case INPUT_ENCODER_CW:
      break;
    
    case INPUT_ENCODER_CCW:
      break;
    
    case INPUT_ENCODER_BTN:
      break;

    case INPUT_KEY:
      switch (evt.key)
      {
        case 0x1B: // Escape
          sceneManager_exit();
          break;

        default:
          break;
      }
      break;

    case INPUT_L_BTN:
      break;
    
    case INPUT_R_BTN:
      break;
  }
}

// Handle input on IR screen
void irHandleInput(InputEvent evt)
{
  switch (evt.type)
  {
    case INPUT_ENCODER_CW:
      break;
    
    case INPUT_ENCODER_CCW:
      break;
    
    case INPUT_ENCODER_BTN:
      break;

    case INPUT_KEY:
      switch (evt.key)
      {
        case 0x1B: // Escape
          sceneManager_exit();
          break;

        default:
          break;
      }
      break;

    case INPUT_L_BTN:
      break;
    
    case INPUT_R_BTN:
      break;
  }
}

// Handle input on settings screen
void settingsHandleInput(InputEvent evt)
{
  switch (evt.type)
  {
    case INPUT_ENCODER_CW:
      break;
    
    case INPUT_ENCODER_CCW:
      break;
    
    case INPUT_ENCODER_BTN:
      break;

    case INPUT_KEY:
      switch (evt.key)
      {
        case 0x1B: // Escape
          sceneManager_exit();
          break;

        default:
          break;
      }
      break;

    case INPUT_L_BTN:
      break;
    
    case INPUT_R_BTN:
      break;
  }
}

// Handle input on WiFi screen
void wifiHandleInput(InputEvent evt)
{
  switch (evt.type)
  {
    case INPUT_ENCODER_CW:
      break;
    
    case INPUT_ENCODER_CCW:
      break;
    
    case INPUT_ENCODER_BTN:
      break;

    case INPUT_KEY:
      switch (evt.key)
      {
        case 0x1B: // Escape
          sceneManager_exit();
          break;

        default:
          break;
      }
      break;

    case INPUT_L_BTN:
      break;
    
    case INPUT_R_BTN:
      break;
  }
}

// Handle input on Bluetooth screen
void btHandleInput(InputEvent evt)
{
  switch (evt.type)
  {
    case INPUT_ENCODER_CW:
      break;
    
    case INPUT_ENCODER_CCW:
      break;
    
    case INPUT_ENCODER_BTN:
      break;

    case INPUT_KEY:
      switch (evt.key)
      {
        case 0x1B: // Escape
          sceneManager_exit();
          break;

        default:
          break;
      }
      break;

    case INPUT_L_BTN:
      break;
    
    case INPUT_R_BTN:
      break;
  }
}

// Handle input on Notes screen
void notesHandleInput(InputEvent evt)
{
  switch (evt.type)
  {
    case INPUT_ENCODER_CW:
      break;
    
    case INPUT_ENCODER_CCW:
      break;
    
    case INPUT_ENCODER_BTN:
      break;

    case INPUT_KEY:
      switch (evt.key)
      {
        case 0x1B: // Escape
          sceneManager_exit();
          break;

        case 0x09: // Tab
          tft.print("Tab");
          break;
        
        case 0x0D: // Enter
          tft.print("\n");
          break;

        // case FN + C = copy

        // case FN + V = paste

        // case FN + S = save to SD card
        // give file name if one has not been assigned

        // case FN + O = open file

        default:
          tft.print(evt.key);
          break;
      }
      break;

    case INPUT_L_BTN:
      break;
    
    case INPUT_R_BTN:
      break;
  }
}

// Hanlde input on Files screen
void filesHandleInput(InputEvent evt)
{
  switch (evt.type)
  {
    case INPUT_ENCODER_CW:
      break;
    
    case INPUT_ENCODER_CCW:
      break;
    
    case INPUT_ENCODER_BTN:
      break;

    case INPUT_KEY:
      switch (evt.key)
      {
        case 0x1B: // Escape
          sceneManager_exit();
          break;

        default:
          break;
      }
      break;

    case INPUT_L_BTN:
      break;
    
    case INPUT_R_BTN:
      break;
  }
}

// Handle input based on the scene that is drawn
void handleInput(SceneType scene, InputEvent evt)
{
  switch (scene)
  {
    case SCENE_TEST1: test1HandleInput(evt); break;
    case SCENE_TEST2: test2HandleInput(evt); break;
    case SCENE_MENU: menuHandleInput(evt); break;
    case SCENE_RFID: rfidHandleInput(evt); break;
    case SCENE_NFC:  nfcHandleInput(evt); break;
    case SCENE_RF:  rfHandleInput(evt); break;
    case SCENE_IR:  irHandleInput(evt); break;
    case SCENE_SETTINGS:  settingsHandleInput(evt); break;
    case SCENE_WIFI:  wifiHandleInput(evt); break;
    case SCENE_BT:  btHandleInput(evt); break;
    case SCENE_NOTES: notesHandleInput(evt); break;
    case SCENE_FILES: filesHandleInput(evt); break;
    // etc.
  }
}

// Function to initialize the TFT as proof of life
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
  tft.printf("%s loading...", scenes[selectedIndex].name);                // Print words at corner
  // tft.drawCircle(TFT_Center_X, TFT_Center_Y, 5, ST77XX_GREEN); // Print a green circle at the center of display
  delay(pdMS_TO_TICKS(1000));
  sceneManager_draw(currentScene);
}


/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=
TFT Task:
This task handles all screen logic.
Designed for TFT ST7789 with 320 x 170 resolution in mind.
Voltage: 3.3V
-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

// Screen Task handle
TaskHandle_t sceneHandle = NULL;

// Main scene task; handles all scene logic
void sceneManager(void *pvParameters)
{ 
  // Run once to retrieve core; debugging
  int core = xPortGetCoreID(); 

  // Main loop
  for (;;)
  {
    InputEvent evt;
    // If an event is in the queue
    if (xQueueReceive(inputQueue, &evt, 0) == pdTRUE)
    {
      handleInput(currentScene, evt); // Handle the input event in the queue
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    // Draw the current scene
    sceneManager_draw(currentScene);
  }

}



// --------------- Main Loops --------------- //

void setup() 
{
 // Initialize serial communication
    Serial.begin(115200);
    inputQueue = xQueueCreate(10, sizeof(InputEvent));

    I2C_init();  // Run once; turn on I2C bus
    TFT_init();  // Run once; turn on TFT
    rEnc_init(); // Run once; turn on rotary encoder
    //buttonLR_init() // Run once; turn on L/R buttons
    //RTC_init(); // Run once; turn on RTC

    

    /*
    Core numbers are assigned as follows:
    0 - Always running, task agnositc.
    1 - Always running, task gnostic.
    */

    // -=-=-=-=-= Core 0 Tasks =-=-=-=-=- //

    // Create the LED task
    xTaskCreatePinnedToCore(
        ledTask,       // Task function
        "Test LED Task",    // Task name
        2048,          // Stack size
        NULL,          // Task parameters
        1,             // Task priority
        &ledTaskHandle,// Task handle
        0              // Core number (0)
    );

     // Create Rotary Encoder Task
    xTaskCreatePinnedToCore(
      rEncTask,               // Task function
      "Rotary Encoder Input", // Task name
      2048,                   // Stack size (in bits?? Bytes?? idk)
      NULL,                   // Task parameters
      1,                      // Task priority
      &rEncHandle,            // Task handle
      0                       // Core number (0)
    );


    // Create Keyboard Task
    xTaskCreatePinnedToCore(
      CardKBTask,           // Task function
      "Keyboard Input",    // Task name
      4096,             // Stack size
      NULL,             // Task parameters
      1,                // Task priority
      &kbHandle,        // Task handle
      0                 // Core number (0)
    );

    // -=-=-=-=-= Core 1 Tasks =-=-=-=-=- //

    // Create TFT task
    xTaskCreatePinnedToCore(
      sceneManager,         // Task function
      "TFT Logic",    // Task name
      8192,           // Stack size
      NULL,           // Task parameters
      1,              // Task priority
      &sceneHandle,  // Task handle
      1               // Core number (1)
    );

    
}

void loop() 
{
  vTaskDelete(NULL);
  // Nothing needs to be here for FreeRTOS.
  // Some people put a vTaskDelay here for stylistic reasons,
  // but I don't think I will.
}




