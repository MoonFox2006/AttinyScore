#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>
//#include <Arduino.h>

enum runstate_t : uint8_t { RUN_IDLE, RUN_LEFT, RUN_RIGHT };
enum buttonstate_t : uint8_t { BTN_RELEASED, BTN_CLICK, BTN_LONGCLICK };

const uint8_t MAX_SCORE = 20;
const uint8_t NORMAL_BRIGHT = 4;
const uint8_t DIM_BRIGHT = 2;
const uint32_t STATE_DURATION = 2000; // 2 sec.

const uint8_t TM_CLK_PIN = PB3;
const uint8_t TM_DIO_PIN = PB4;

const uint8_t BTN_PINS[2] = { PB2, PB1 };

const uint32_t DEBOUNCE_TIME = 50; // 50 ms.
const uint32_t LONGCLICK_TIME = 500; // 0.5 sec.

volatile buttonstate_t buttons[2] = { BTN_RELEASED, BTN_RELEASED };
uint8_t score[2] = { MAX_SCORE, MAX_SCORE };
volatile uint32_t _ms = 0;
runstate_t runstate = RUN_IDLE;

static inline uint32_t millis() {
  return _ms;
}

ISR(TIM0_COMPA_vect) {
  ++_ms;
}

ISR(PCINT0_vect) {
  static uint32_t pressedTimes[2] = { 0, 0 };

  uint8_t pinb = PINB;

  for (uint8_t i = 0; i < 2; ++i) {
    if (pinb & (1 << BTN_PINS[i])) { // Button released
      if (pressedTimes[i]) { // Was pressed
        uint32_t time = millis() - pressedTimes[i];

        if (time >= LONGCLICK_TIME) // Long click
          buttons[i] = BTN_LONGCLICK;
        else if (time >= DEBOUNCE_TIME) // Click
          buttons[i] = BTN_CLICK;
/*
        else
          buttons[i] = BTN_RELEASED;
*/
        pressedTimes[i] = 0;
      }
    } else { // Button pressed
      if (! pressedTimes[i]) { // Was released
        pressedTimes[i] = millis();
      }
    }
  }
}

static inline void _bitDelay() {
/*
  delayMicroseconds(50);
*/
  _delay_us(50);
}

static void _start() {
/*
  digitalWrite(TM_CLK_PIN, HIGH);
  digitalWrite(TM_DIO_PIN, HIGH);
  digitalWrite(TM_DIO_PIN, LOW);
  digitalWrite(TM_CLK_PIN, LOW);
*/
  PORTB |= (1 << TM_CLK_PIN);
  PORTB |= (1 << TM_DIO_PIN);
  PORTB &= ~(1 << TM_DIO_PIN);
  PORTB &= ~(1 << TM_CLK_PIN);
}

static void _stop() {
/*
  digitalWrite(TM_CLK_PIN, LOW);
  digitalWrite(TM_DIO_PIN, LOW);
  digitalWrite(TM_CLK_PIN, HIGH);
  digitalWrite(TM_DIO_PIN, HIGH);
*/
  PORTB &= ~(1 << TM_CLK_PIN);
  PORTB &= ~(1 << TM_DIO_PIN);
  PORTB |= (1 << TM_CLK_PIN);
  PORTB |= (1 << TM_DIO_PIN);
}

static void _writeByte(uint8_t data) {
  for (uint8_t i = 0; i < 8; ++i) {
/*
    digitalWrite(TM_CLK_PIN, LOW);
    digitalWrite(TM_DIO_PIN, data & 0x01);
*/
    PORTB &= ~(1 << TM_CLK_PIN);
    if (data & 0x01)
      PORTB |= (1 << TM_DIO_PIN);
    else
      PORTB &= ~(1 << TM_DIO_PIN);
    data >>= 1;
/*
    digitalWrite(TM_CLK_PIN, HIGH);
*/
    PORTB |= (1 << TM_CLK_PIN);
  }
/*
  digitalWrite(TM_CLK_PIN, LOW); // wait for the ACK
  digitalWrite(TM_DIO_PIN, HIGH);
  digitalWrite(TM_CLK_PIN, HIGH);
  pinMode(TM_DIO_PIN, INPUT);
*/
  PORTB &= ~(1 << TM_CLK_PIN);
  PORTB |= (1 << TM_DIO_PIN);
  PORTB |= (1 << TM_CLK_PIN);
  DDRB &= ~(1 << TM_DIO_PIN);
  PORTB &= ~(1 << TM_DIO_PIN);
  _bitDelay();
/*
  if (! digitalRead(TM_DIO_PIN)) {
    pinMode(TM_DIO_PIN, OUTPUT);
    digitalWrite(TM_DIO_PIN, LOW);
  }
*/
  if (! ((PINB >> TM_DIO_PIN) & 0x01)) {
    DDRB |= (1 << TM_DIO_PIN);
//    PORTB &= ~(1 << TM_DIO_PIN);
  }
  _bitDelay();
/*
  pinMode(TM_DIO_PIN, OUTPUT);
*/
  DDRB |= (1 << TM_DIO_PIN);
  _bitDelay();
}

static void display(const uint8_t *segments, uint8_t brightness) {
  const uint8_t ADDR_AUTO = 0x40;
  const uint8_t STARTADDR = 0xC0;

  _start();
  _writeByte(ADDR_AUTO);
  _stop();
  _start();
  _writeByte(STARTADDR);
  for (uint8_t i = 0; i < 4; ++i) {
    _writeByte(segments[i]);
  }
  _stop();
  _start();
  _writeByte(0x88 | brightness);
  _stop();
}

int main() {
/***
 * setup()
 */

/*
  pinMode(TM_CLK_PIN, OUTPUT);
  pinMode(TM_DIO_PIN, OUTPUT);
*/
  DDRB |= ((1 << TM_CLK_PIN) | (1 << TM_DIO_PIN));
  for (uint8_t i = 0; i < 2; ++i) {
/*
    pinMode(BTN_PINS[i], INPUT_PULLUP);
*/
//    DDRB &= ~(1 << BTN_PINS[i]);
    PORTB |= (1 << BTN_PINS[i]);
    PCMSK |= (1 << BTN_PINS[i]);
  }
  GIMSK |= (1 << PCIE);
  TCCR0A = 1 << WGM01; // CTC mode
  TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler /64
  OCR0A = 149; // 150 - 1
  TIMSK0 = 1 << OCIE0A;
//  TCNT0 = 0;
  sei();
  set_sleep_mode(SLEEP_MODE_IDLE);

/***
 * loop()
 */

  for (;;) {
    static uint32_t stateTime = 0;
    static uint8_t brightness = DIM_BRIGHT;

    uint32_t uptime = millis();

    for (uint8_t i = 0; i < 2; ++i) {
      if (buttons[i] != BTN_RELEASED) {
        if (runstate == RUN_IDLE) {
          brightness = NORMAL_BRIGHT;
          runstate = (runstate_t)(RUN_LEFT + i);
        } else {
          if (buttons[i] == BTN_CLICK) {
            if (i) { // +
              if (score[runstate - RUN_LEFT] < 99)
                ++score[runstate - RUN_LEFT];
            } else { // -
              if (score[runstate - RUN_LEFT])
                --score[runstate - RUN_LEFT];
            }
          } else { // Long click
            score[runstate - RUN_LEFT] = i ? MAX_SCORE : 0;
          }
        }
        stateTime = uptime;
        buttons[i] = BTN_RELEASED;
      }
    }

    if ((runstate != RUN_IDLE) && (uptime - stateTime >= STATE_DURATION)) {
      brightness = DIM_BRIGHT;
      runstate = RUN_IDLE;
    }

    {
      const uint8_t DIGITS[10] = {
        0B00111111, 0B00000110, 0B01011011, 0B01001111, 0B01100110, 0B01101101, 0B01111101, 0B0000111, 0B01111111, 0B01101111
      };
      const uint8_t MINUS = 0B01000000;
      const uint8_t DOT = 0B10000000;

      uint8_t segments[4];

      for (uint8_t i = 0; i < 2; ++i) {
        bool draw = (runstate != RUN_LEFT + i) || (uptime % 500 < 250);

        if (draw) {
          if (score[i]) {
            segments[i * 2] = DIGITS[score[i] / 10];
            segments[i * 2 + 1] = DIGITS[score[i] % 10] | DOT;
          } else {
            segments[i * 2] = MINUS;
            segments[i * 2 + 1] = MINUS | DOT;
          }
        } else {
          segments[i * 2] = 0;
          segments[i * 2 + 1] = DOT;
        }
      }
      display(segments, brightness);
    }

    sleep_mode();
  }
}
