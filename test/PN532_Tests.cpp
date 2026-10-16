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

// -------------------- PN532 NFC -------------------- //

// Relevant Definitions
// Operating Voltage: 5V
// SDA = GPIO8 by default
// SCL = GPIO9 by default
const int PN532_ADDR = 0x24; // Default I2C address for PN532

Adafruit_PN532 nfc(-1, -1); // I2C mode, no IRQ/reset pins wired

void PN_init()
{
  Serial.begin(115200);
  Wire.begin(8, 9); // Initialize I2C (default pins GPIO 8 & 9)
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

void PN_scan()
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
}
// -------------------- Main Functions -------------------- //
void setup() {
  PN_init();
}

void loop() {
  PN_scan();
  delay(10);
}
