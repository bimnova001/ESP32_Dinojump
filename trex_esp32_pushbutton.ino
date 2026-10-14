#include <Arduino.h>
#include <Wire.h>

// ESP32 + SSD1306 OLED pins
#define TREX_SDA_PIN 21           
#define TREX_SCL_PIN 22

// Push button pin
// Connect one side of button to GPIO18 and the other side to GND.
#define GAME_BUTTON_PIN 18
// The original T-Rex file uses TOUCH_SENSOR_PIN internally,
// so we map it to our push button pin.
#define TOUCH_SENSOR_PIN GAME_BUTTON_PIN

// Pressed = LOW because we use INPUT_PULLUP
#define BUTTON_ACTIVE_HIGH 0

// Optional buzzer pin. Leave it disconnected if you do not want sound.
#define BUZZER_PIN 12

#include "t-rex-duino.h"

void setup() {
  Serial.begin(115200);

  // Your OLED is I2C SSD1306, confirmed address 0x3C.
  Wire.begin(TREX_SDA_PIN, TREX_SCL_PIN);
  // Wire.setClock(400000);

  // Push button between GPIO18 and GND. No external resistor needed.
  pinMode(GAME_BUTTON_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  dino_enter();
}

void loop() {
  runDinoGame();
}
