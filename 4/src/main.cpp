#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

enum runstate_t : uint8_t { RUN_IDLE, RUN_LEFT, RUN_RIGHT };

const uint8_t MAX_SCORE = 20;
const uint8_t NORMAL_BRIGHT = 4;
const uint8_t DIM_BRIGHT = 2;
const uint16_t STATE_DURATION = 2000; // 2 sec.

const uint8_t TM_CLK_PIN = PB3;
const uint8_t TM_DIO_PIN = PB4;

const uint8_t BTN_PINS[2] = { PB2, PB1 };

const uint16_t DEBOUNCE_TIME = 50; // 50 ms.
const uint16_t HOLD_TIME = 500; // 0.5 sec.
const uint16_t REPEAT_TIME = 200; // 0.2 sec.

volatile uint8_t score[2] = { MAX_SCORE, MAX_SCORE };
volatile uint16_t _ms = 0;
volatile uint16_t stateTime = 0;
volatile runstate_t runstate = RUN_IDLE;
volatile uint8_t brightness = DIM_BRIGHT;

inline uint16_t millis() {
  return _ms;
}

ISR(TIM0_COMPA_vect) {
  static uint16_t pressedTime[2] = { 0, 0 };

  uint8_t pinb;

  ++_ms;
  pinb = PINB;
  for (uint8_t i = 0; i < 2; ++i) {
    if (! (pinb & (1 << BTN_PINS[i]))) { // Button pressed
      if (pressedTime[i] < 0xFFFF)
        ++pressedTime[i];
      if (pressedTime[i] >= DEBOUNCE_TIME) {
        if (i && (pressedTime[0] >= DEBOUNCE_TIME)) { // Both buttons pressed, reset score
          score[0] = score[1] = MAX_SCORE;
          runstate = RUN_IDLE;
          brightness = DIM_BRIGHT;
        } else {
          if ((pressedTime[i] == DEBOUNCE_TIME) || ((pressedTime[i] >= HOLD_TIME) && ((pressedTime[i] - HOLD_TIME) % REPEAT_TIME == 0))) { // Click
            if (runstate == RUN_IDLE) {
              runstate = (runstate_t)(RUN_LEFT + i);
              brightness = NORMAL_BRIGHT;
            } else {
              if (i) { // +
                if (score[runstate - RUN_LEFT] < 99)
                  ++score[runstate - RUN_LEFT];
              } else { // -
                if (score[runstate - RUN_LEFT])
                  --score[runstate - RUN_LEFT];
              }
            }
            stateTime = millis();
          }
        }
      }
    } else { // Button released
      pressedTime[i] = 0;
    }
  }
}

static inline void _bitDelay() {
  _delay_us(50);
}

static void _start() {
//  digitalWrite(TM_CLK_PIN, HIGH);
//  digitalWrite(TM_DIO_PIN, HIGH);
  PORTB |= ((1 << TM_CLK_PIN) | (1 << TM_DIO_PIN));
//  digitalWrite(TM_DIO_PIN, LOW);
//  digitalWrite(TM_CLK_PIN, LOW);
  PORTB &= ~(1 << TM_DIO_PIN);
  PORTB &= ~(1 << TM_CLK_PIN);
}

static void _stop() {
//  digitalWrite(TM_CLK_PIN, LOW);
//  digitalWrite(TM_DIO_PIN, LOW);
  PORTB &= ~((1 << TM_CLK_PIN) | (1 << TM_DIO_PIN));
//  digitalWrite(TM_CLK_PIN, HIGH);
//  digitalWrite(TM_DIO_PIN, HIGH);
  PORTB |= (1 << TM_CLK_PIN);
  PORTB |= (1 << TM_DIO_PIN);
}

static void _writeByte(uint8_t data) {
  for (uint8_t i = 0; i < 8; ++i) {
//    digitalWrite(TM_CLK_PIN, LOW);
    PORTB &= ~(1 << TM_CLK_PIN);
//    digitalWrite(TM_DIO_PIN, data & 0x01);
    if (data & 0x01)
      PORTB |= (1 << TM_DIO_PIN);
    else
      PORTB &= ~(1 << TM_DIO_PIN);
    data >>= 1;
//    digitalWrite(TM_CLK_PIN, HIGH);
    PORTB |= (1 << TM_CLK_PIN);
  }
//  digitalWrite(TM_CLK_PIN, LOW); // wait for the ACK
  PORTB &= ~(1 << TM_CLK_PIN);
//  digitalWrite(TM_DIO_PIN, HIGH);
  PORTB |= (1 << TM_DIO_PIN);
//  digitalWrite(TM_CLK_PIN, HIGH);
  PORTB |= (1 << TM_CLK_PIN);
//  pinMode(TM_DIO_PIN, INPUT);
  DDRB &= ~(1 << TM_DIO_PIN);
  PORTB &= ~(1 << TM_DIO_PIN);
  _bitDelay();
//  if (! digitalRead(TM_DIO_PIN)) {
  if (! ((PINB >> TM_DIO_PIN) & 0x01)) {
//    pinMode(TM_DIO_PIN, OUTPUT);
    DDRB |= (1 << TM_DIO_PIN);
//    digitalWrite(TM_DIO_PIN, LOW);
//    PORTB &= ~(1 << TM_DIO_PIN);
  }
  _bitDelay();
//  pinMode(TM_DIO_PIN, OUTPUT);
  DDRB |= (1 << TM_DIO_PIN);
  _bitDelay();
}

static void display(const uint8_t *segments) {
  const uint8_t ADDR_AUTO = 0x40;
  const uint8_t STARTADDR = 0xC0;

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
  _writeByte(0x88 | brightness);
  _stop();
}

int main() {
/***
 * setup()
 */

//  pinMode(TM_CLK_PIN, OUTPUT);
//  pinMode(TM_DIO_PIN, OUTPUT);
  DDRB |= ((1 << TM_CLK_PIN) | (1 << TM_DIO_PIN));
  for (uint8_t i = 0; i < 2; ++i) {
//    pinMode(BTN_PINS[i], INPUT_PULLUP);
    DDRB &= ~(1 << BTN_PINS[i]);
    PORTB |= (1 << BTN_PINS[i]);
  }
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
    uint8_t segments[4];
    uint16_t uptime = millis();

    if ((runstate != RUN_IDLE) && (uptime - stateTime >= STATE_DURATION)) {
      runstate = RUN_IDLE;
      brightness = DIM_BRIGHT;
    }

    for (uint8_t i = 0; i < 2; ++i) {
      static const uint8_t DIGITS[10] = {
        0B00111111, 0B00000110, 0B01011011, 0B01001111, 0B01100110, 0B01101101, 0B01111101, 0B0000111, 0B01111111, 0B01101111
      };
      const uint8_t MINUS = 0B01000000;
      const uint8_t DOT = 0B10000000;

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
    display(segments);

    sleep_mode();
  }
}
