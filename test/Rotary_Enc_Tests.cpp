// -------------------- Libraries -------------------- //
#include <AiEsp32RotaryEncoder.h>


// -------------------- Rotary Encoder -------------------- //
// R(otary) ENC(oder) Definitions
#define RENC_CLK  5
#define RENC_DT  15
#define RENC_SW  16
#define RENC_VCC  -1
#define RENC_step  4

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(RENC_DT, RENC_CLK, RENC_SW, RENC_VCC, RENC_step);

void rotary_onButtonClick()
{
    static unsigned long lastTimePressed = 0; // Soft debouncing
    if (millis() - lastTimePressed < 500)
    {
      return;
    }
    lastTimePressed = millis();
    Serial.print("Button pressed!\n");
}

void rotary_loop()
{
    if (rotaryEncoder.encoderChanged())
    {
      Serial.print("Value: ");
      Serial.println(rotaryEncoder.readEncoder());
    }
    if (rotaryEncoder.isEncoderButtonClicked())
    {
      rotary_onButtonClick();
    }
}

void IRAM_ATTR readEncoderISR()
{
    rotaryEncoder.readEncoder_ISR();
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Serial Monitor Running!");

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);

  bool rollover = true;
  rotaryEncoder.setBoundaries(0, 1000, rollover);

  rotaryEncoder.disableAcceleration();
}

void loop()
{
  rotary_loop();
  delay(1); 
}
