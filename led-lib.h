#ifndef LEDINTR_H
#define LEDINTR_H

#include <Arduino.h>

// Универсальный интерфейс GPIO
class IGpioAdapter {
public:
    virtual void pinWrite(int pin, bool value) = 0;
    virtual bool pinRead(int pin) = 0;
    virtual void delay(unsigned long ms) = 0;
    virtual void delayMicro(unsigned long us) = 0;
    virtual ~IGpioAdapter() {}
};

// Кодировщик / декодировщик
class LedIntr {
public:
    LedIntr(IGpioAdapter* adapter, int txPin = -1, int rxPin = -1, unsigned long bitDurationUs = 1000);

    void send(const String& text);

    void setBitDuration(unsigned long bitDurationUs);
    unsigned long bitDuration() const { return _bitDurationUs; }

    void setThreshold(uint16_t threshold);
    uint16_t threshold() const { return _threshold; }

    void setSamplesPerBit(size_t samples);
    size_t samplesPerBit() const { return _samplesPerBit; }

    void autoCalibrate(size_t sampleCount = 250, unsigned long sampleDelayUs = 500);

    bool receiveMessage(String& out, size_t maxChars = 64, unsigned long startTimeoutMs = 5000);

    void receiveLoop(Stream& output = Serial);

private:
    IGpioAdapter* _adapter;
    int _txPin;
    int _rxPin;
    unsigned long _bitDurationUs;
    uint16_t _threshold;
    size_t _samplesPerBit;
    unsigned long _sampleSpacingUs;
    unsigned long _pollDelayUs;

    void sendBit(uint8_t bit);
    uint16_t sampleAnalog();
    void waitInterval(unsigned long durationUs);
    void updateTiming();
    bool waitForStartBit(unsigned long timeoutMs);
    bool receiveByte(uint8_t& byteOut, unsigned long startTimeoutMs);
};

#endif
