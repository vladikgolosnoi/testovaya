#include "led-lib.h"
#include <Arduino.h>

namespace {
constexpr unsigned long kDefaultBitDurationUs = 1000;   // 1 кГц
constexpr uint16_t kDefaultThreshold = 2048;            // середина шкалы ESP32 (0-4095)
constexpr size_t kDefaultSamplesPerBit = 8;
constexpr unsigned long kMinSampleSpacingUs = 5;
constexpr unsigned long kMinPollDelayUs = 50;
}

LedIntr::LedIntr(IGpioAdapter* adapter, int txPin, int rxPin, unsigned long bitDurationUs)
    : _adapter(adapter),
      _txPin(txPin),
      _rxPin(rxPin),
      _bitDurationUs(bitDurationUs == 0 ? kDefaultBitDurationUs : bitDurationUs),
      _threshold(kDefaultThreshold),
      _samplesPerBit(kDefaultSamplesPerBit),
      _sampleSpacingUs(kMinSampleSpacingUs),
      _pollDelayUs(kMinPollDelayUs) {
    if (_txPin != -1) {
        pinMode(_txPin, OUTPUT);
        digitalWrite(_txPin, LOW);
    }
    if (_rxPin != -1) {
        pinMode(_rxPin, INPUT);
    }
    updateTiming();
}

void LedIntr::updateTiming() {
    if (_samplesPerBit == 0) {
        _samplesPerBit = 1;
    }

    _sampleSpacingUs = _bitDurationUs / (_samplesPerBit + 1);
    if (_sampleSpacingUs < kMinSampleSpacingUs) {
        _sampleSpacingUs = kMinSampleSpacingUs;
    }

    _pollDelayUs = _bitDurationUs / 4;
    if (_pollDelayUs < kMinPollDelayUs) {
        _pollDelayUs = kMinPollDelayUs;
    }
}

void LedIntr::setBitDuration(unsigned long bitDurationUs) {
    if (bitDurationUs == 0) {
        return;
    }
    _bitDurationUs = bitDurationUs;
    updateTiming();
}

void LedIntr::setThreshold(uint16_t threshold) {
    _threshold = threshold;
}

void LedIntr::setSamplesPerBit(size_t samples) {
    if (samples == 0) {
        samples = 1;
    }
    _samplesPerBit = samples;
    updateTiming();
}

void LedIntr::waitInterval(unsigned long durationUs) {
    if (!_adapter || durationUs == 0) {
        return;
    }

    if (durationUs >= 1000) {
        unsigned long ms = durationUs / 1000;
        unsigned long rem = durationUs % 1000;
        if (ms > 0) {
            _adapter->delay(ms);
        }
        if (rem > 0) {
            _adapter->delayMicro(rem);
        }
    } else {
        _adapter->delayMicro(durationUs);
    }
}

uint16_t LedIntr::sampleAnalog() {
    if (_rxPin == -1) {
        return 0;
    }

    if (_samplesPerBit <= 1) {
        return static_cast<uint16_t>(analogRead(_rxPin));
    }

    uint32_t total = 0;
    for (size_t i = 0; i < _samplesPerBit; ++i) {
        total += analogRead(_rxPin);
        if (i + 1 < _samplesPerBit) {
            waitInterval(_sampleSpacingUs);
        }
    }

    return static_cast<uint16_t>(total / _samplesPerBit);
}

void LedIntr::autoCalibrate(size_t sampleCount, unsigned long sampleDelayUs) {
    if (_rxPin == -1 || sampleCount == 0) {
        return;
    }

    uint16_t minValue = UINT16_MAX;
    uint16_t maxValue = 0;

    for (size_t i = 0; i < sampleCount; ++i) {
        uint16_t value = sampleAnalog();
        if (value < minValue) {
            minValue = value;
        }
        if (value > maxValue) {
            maxValue = value;
        }
        if (sampleDelayUs > 0 && i + 1 < sampleCount) {
            waitInterval(sampleDelayUs);
        }
    }

    if (maxValue - minValue < 10) {
        _threshold = minValue;
    } else {
        _threshold = static_cast<uint16_t>((minValue + maxValue) / 2);
    }
}

void LedIntr::sendBit(uint8_t bit) {
    if (_txPin == -1 || !_adapter) {
        return;
    }

    _adapter->pinWrite(_txPin, bit != 0);
    waitInterval(_bitDurationUs);
}

void LedIntr::send(const String& text) {
    if (_txPin == -1) {
        return;
    }

    for (size_t i = 0; i < text.length(); ++i) {
        uint8_t byteVal = static_cast<uint8_t>(text[i]);

        // стартовый бит (0)
        sendBit(0);

        // биты данных отправляются от младшего к старшему (LSB first)
        for (uint8_t bit = 0; bit < 8; ++bit) {
            sendBit((byteVal >> bit) & 0x01);
        }

        // стоповый бит (1)
        sendBit(1);

        // пауза между символами (1 бит)
        waitInterval(_bitDurationUs);
    }

    // Линия в состояние покоя
    sendBit(1);
}

bool LedIntr::waitForStartBit(unsigned long timeoutMs) {
    if (_rxPin == -1) {
        return false;
    }

    unsigned long start = millis();
    const size_t requiredSamples = (timeoutMs == 0) ? 1 : 3;
    size_t consecutiveLow = 0;

    while (true) {
        uint16_t level = sampleAnalog();
        if (level <= _threshold) {
            ++consecutiveLow;
            if (consecutiveLow >= requiredSamples) {
                waitInterval(_bitDurationUs / 2);
                return true;
            }
        } else {
            consecutiveLow = 0;
        }

        if (timeoutMs == 0) {
            return false;
        }

        if (timeoutMs > 0 && (millis() - start) >= timeoutMs) {
            return false;
        }

        waitInterval(_pollDelayUs);
    }
}

bool LedIntr::receiveByte(uint8_t& byteOut, unsigned long startTimeoutMs) {
    if (!waitForStartBit(startTimeoutMs)) {
        return false;
    }

    // смещаемся к середине первого бита данных
    waitInterval(_bitDurationUs);

    uint8_t value = 0;
    for (uint8_t bit = 0; bit < 8; ++bit) {
        uint16_t level = sampleAnalog();
        if (level > _threshold) {
            value |= (1u << bit);
        }
        waitInterval(_bitDurationUs);
    }

    uint16_t stopLevel = sampleAnalog();
    if (stopLevel <= _threshold) {
        // позволяем линии восстановиться
        waitInterval(_bitDurationUs);
        return false;
    }

    waitInterval(_bitDurationUs / 2);
    byteOut = value;
    return true;
}

bool LedIntr::receiveMessage(String& out, size_t maxChars, unsigned long startTimeoutMs) {
    if (_rxPin == -1 || maxChars == 0) {
        return false;
    }

    String buffer;
    buffer.reserve(maxChars);

    while (buffer.length() < maxChars) {
        uint8_t byteVal = 0;
        if (!receiveByte(byteVal, startTimeoutMs)) {
            break;
        }

        buffer += static_cast<char>(byteVal);

        if (byteVal == '\n') {
            break;
        }
    }

    if (buffer.length() == 0) {
        return false;
    }

    out = buffer;
    return true;
}

void LedIntr::receiveLoop(Stream& output) {
    if (_rxPin == -1) {
        return;
    }

    while (true) {
        uint8_t byteVal = 0;
        if (receiveByte(byteVal, _bitDurationUs / 1000 + 2)) {
            output.write(byteVal);
        } else {
            waitInterval(_pollDelayUs);
        }

#if defined(ARDUINO_ARCH_ESP32)
        // не блокируем планировщик на ESP32
        yield();
#endif
    }
}
