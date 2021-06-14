#pragma once

#include <Arduino.h>

template<const uint8_t CLK_PIN, const uint8_t DIO_PIN>
class TM1637 {
public:
  static const uint8_t MINUS = 0B01000000;
  static const uint8_t DOT = 0B10000000;

  TM1637(uint8_t brightness = 4);

  void setBrightness(uint8_t brightness) {
    _brightness = brightness < 7 ? brightness : 7;
  }
  void clear() {
    display(0);
  }
  void display(uint8_t pos, uint8_t segments);
  void display(const uint8_t *segments);
  void display(uint32_t segments);
  void displayNum(int16_t num, bool leadingZero = false);
  void operator=(int16_t num) {
    displayNum(num);
  }

  static uint8_t digitToSegments(int8_t digit);

protected:
  static const uint8_t ADDR_AUTO = 0x40;
  static const uint8_t ADDR_FIXED = 0x44;
  static const uint8_t STARTADDR = 0xC0;

  void _bitDelay();
  void _start();
  void _stop();
  bool _writeByte(uint8_t data);

  uint8_t _brightness;
};

template<const uint8_t CLK_PIN, const uint8_t DIO_PIN>
TM1637<CLK_PIN, DIO_PIN>::TM1637(uint8_t brightness) {
  pinMode(CLK_PIN, OUTPUT);
  pinMode(DIO_PIN, OUTPUT);
  _brightness = brightness < 7 ? brightness : 7;
}

template<const uint8_t CLK_PIN, const uint8_t DIO_PIN>
void TM1637<CLK_PIN, DIO_PIN>::display(uint8_t pos, uint8_t segments) {
  if (pos < 4) {
    _start();
    _writeByte(ADDR_FIXED);
    _stop();
    _start();
    _writeByte(STARTADDR + pos);
    _writeByte(segments);
    _stop();
    _start();
    _writeByte(0x88 | _brightness);
    _stop();
  }
}

template<const uint8_t CLK_PIN, const uint8_t DIO_PIN>
void TM1637<CLK_PIN, DIO_PIN>::display(const uint8_t *segments) {
  _start();
  _writeByte(ADDR_AUTO);
  _stop();
  _start();
  _writeByte(STARTADDR);
  for (int8_t i = 0; i < 4; ++i) {
    _writeByte(segments[i]);
  }
  _stop();
  _start();
  _writeByte(0x88 | _brightness);
  _stop();
}

template<const uint8_t CLK_PIN, const uint8_t DIO_PIN>
void TM1637<CLK_PIN, DIO_PIN>::display(uint32_t segments) {
  _start();
  _writeByte(ADDR_AUTO);
  _stop();
  _start();
  _writeByte(STARTADDR);
  for (int8_t i = 0; i < 4; ++i) {
    _writeByte(segments >> (8 * i));
  }
  _stop();
  _start();
  _writeByte(0x88 | _brightness);
  _stop();
}

template<const uint8_t CLK_PIN, const uint8_t DIO_PIN>
void TM1637<CLK_PIN, DIO_PIN>::displayNum(int16_t num, bool leadingZero) {
  uint32_t data;

  if ((num < -999) || (num > 9999)) {
    data = 0x50791C3F; // Over
  } else {
    bool minus = num < 0;

    if (minus)
      num = -num;
    data = (uint32_t)digitToSegments(num % 10) << 24;
    num /= 10;
    if (leadingZero || (num != 0))
      data |= (uint32_t)digitToSegments(num % 10) << 16;
    num /= 10;
    if (leadingZero || (num != 0))
      data |= (uint32_t)digitToSegments(num % 10) << 8;
    num /= 10;
    if (minus) {
      data |= MINUS;
    } else {
      if (leadingZero || (num != 0))
        data |= digitToSegments(num % 10);
    }
  }
  display(data);
}

template<const uint8_t CLK_PIN, const uint8_t DIO_PIN>
uint8_t TM1637<CLK_PIN, DIO_PIN>::digitToSegments(int8_t digit) {
  static const uint8_t DIGITS[10] PROGMEM = {
    0B00111111, 0B00000110, 0B01011011, 0B01001111, 0B01100110, 0B01101101, 0B01111101, 0B0000111, 0B01111111, 0B01101111
  };

  if (digit < 0)
    return MINUS;
  if (digit < 10)
    return pgm_read_byte(&DIGITS[digit]);
  return 0;
}

template<const uint8_t CLK_PIN, const uint8_t DIO_PIN>
inline void TM1637<CLK_PIN, DIO_PIN>::_bitDelay() {
  delayMicroseconds(50);
}

template<const uint8_t CLK_PIN, const uint8_t DIO_PIN>
void TM1637<CLK_PIN, DIO_PIN>::_start() {
  digitalWrite(CLK_PIN, HIGH);
  digitalWrite(DIO_PIN, HIGH);
  digitalWrite(DIO_PIN, LOW);
  digitalWrite(CLK_PIN, LOW);
}

template<const uint8_t CLK_PIN, const uint8_t DIO_PIN>
void TM1637<CLK_PIN, DIO_PIN>::_stop() {
  digitalWrite(CLK_PIN, LOW);
  digitalWrite(DIO_PIN, LOW);
  digitalWrite(CLK_PIN, HIGH);
  digitalWrite(DIO_PIN, HIGH);
}

template<const uint8_t CLK_PIN, const uint8_t DIO_PIN>
bool TM1637<CLK_PIN, DIO_PIN>::_writeByte(uint8_t data) {
  for (uint8_t i = 0; i < 8; ++i) {
    digitalWrite(CLK_PIN, LOW);
    digitalWrite(DIO_PIN, data & 0x01);
    data >>= 1;
    digitalWrite(CLK_PIN, HIGH);
  }
  digitalWrite(CLK_PIN, LOW); // wait for the ACK
  digitalWrite(DIO_PIN, HIGH);
  digitalWrite(CLK_PIN, HIGH);
  pinMode(DIO_PIN, INPUT);
  _bitDelay();

  bool ack = digitalRead(DIO_PIN);

  if (! ack) {
    pinMode(DIO_PIN, OUTPUT);
    digitalWrite(DIO_PIN, LOW);
  }
  _bitDelay();
  pinMode(DIO_PIN, OUTPUT);
  _bitDelay();
  return ack;
}
