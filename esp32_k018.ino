#include <Arduino.h>
#include "Arduino-lib.h"
#include "led-lib.h"

ArduinoAdapter adapter;

constexpr int TX_PIN = -1;         // отключаем передачу, ESP32 работает как приёмник
constexpr int RX_PIN = 34;         // фоторезисторный делитель на входе ADC1_CH6
constexpr unsigned long BIT_US = 1200; // длительность бита в микросекундах (~833 бод)

LedIntr dev(&adapter, TX_PIN, RX_PIN, BIT_US);

void setup() {
  Serial.begin(115200);

  if (TX_PIN != -1) {
    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, LOW);
  }

  dev.setSamplesPerBit(6);
  delay(500); // даём питанию и датчику стабилизироваться
  dev.autoCalibrate(400, 1000);

  Serial.println(F("--- ESP32 LED receiver ready ---"));
  Serial.print(F("Threshold: "));
  Serial.println(dev.threshold());
}

void loop() {
  String received;
  if (dev.receiveMessage(received, 64, 2)) {
    Serial.print(F("RX: "));
    Serial.println(received);
  }

  delay(5); // сглаживаем опрос и даём времени другим задачам
}
