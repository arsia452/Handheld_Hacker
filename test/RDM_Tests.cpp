
/// -------------------- Libraries -------------------- //
#include <SoftwareSerial.h>
#include <HardwareSerial.h>
// -------------------- RDM6300 RFID -------------------- //
// Relevant RDM Definitions

// RDM6300
#define RDM_RX_PIN 1   // Safe GPIO; ESP32 PIN 4 <-> RDM TX PIN
#define RDM_TX_PIN -1  // No actual connection here; just for func def

HardwareSerial rdmSerial(2);  // UART2

void RFID_init() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting...");
  rdmSerial.begin(9600, SERIAL_8N1, RDM_RX_PIN, RDM_TX_PIN);
  Serial.println("RDM6300 ready.");
}

void RFID_scan() {
  while (rdmSerial.available()) {
    byte b = rdmSerial.read();
    Serial.printf("Got byte: 0x%02X\n", b);
  }
}

// -------------------- RDM6300 RFID End -------------------- //

// -------------------- Main Functions -------------------- //
void setup() {
  RFID_init();
}

void loop() {
  RFID_scan();
  delay(10);
}

