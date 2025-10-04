#include <Arduino.h>
#include "Arduino-lib.h"
#include "led-lib.h"

ArduinoAdapter adapter;

constexpr int TX_PIN = 13;    // светодиод или лазер
constexpr int RX_PIN = 34;    // фоторезисторный делитель на ADC1_CH6
constexpr unsigned long BIT_US = 1200; // длительность бита в микросекундах

LedIntr dev(&adapter, TX_PIN, RX_PIN, BIT_US);

void setup() {
  Serial.begin(115200);

  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, LOW);

  dev.setSamplesPerBit(6);
  delay(500); // стабилизируем питание датчика
  dev.autoCalibrate(400, 1000);

  Serial.println(F("--- ESP32 LED link ready ---"));
  Serial.print(F("Threshold: "));
  Serial.println(dev.threshold());
}

void loop() {
  if (Serial.available() > 0) {
    String input =ч] Serial.readStringUntil('\n');
    Serial.print(F("TX: "));
    Serial.println(input);

    dev.send(input + '\n');
  }

  String received;
  if (dev.receiveMessage(received, 64, 2)) {
    Serial.print(F("RX: "));
    Serial.println(received);
  }
}
