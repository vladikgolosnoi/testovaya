#include <Arduino.h>
#include "Arduino-lib.h"
#include "led-lib.h"

ArduinoAdapter adapter;
// txPin = 13, без rx
LedIntr dev(&adapter, 13, -1, 100);

void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(115200);
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    Serial.print("Получено: ");
    Serial.println(input);

    dev.send(input);
    Serial.print("Sent: ");
    Serial.println(input);
  }
}
