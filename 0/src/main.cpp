#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <Arduino.h>
#include "TM1637.h"

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

TM1637<TM_CLK_PIN, TM_DIO_PIN> display(DIM_BRIGHT);
volatile buttonstate_t buttons[2] = { BTN_RELEASED, BTN_RELEASED };
uint8_t score[2] = { MAX_SCORE, MAX_SCORE };
runstate_t runstate = RUN_IDLE;

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

void setup() {
  for (uint8_t i = 0; i < 2; ++i) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
    PCMSK |= (1 << BTN_PINS[i]);
  }
  GIMSK |= (1 << PCIE);
  sei();
  set_sleep_mode(SLEEP_MODE_IDLE);
}

void loop() {
  static uint32_t stateTime = 0;

  uint32_t uptime = millis();

  for (uint8_t i = 0; i < 2; ++i) {
    if (buttons[i] != BTN_RELEASED) {
      if (runstate == RUN_IDLE) {
        display.setBrightness(NORMAL_BRIGHT);
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
    display.setBrightness(DIM_BRIGHT);
    runstate = RUN_IDLE;
  }

  for (uint8_t i = 0; i < 2; ++i) {
    bool draw = (runstate != RUN_LEFT + i) || (uptime % 500 < 250);

    if (draw) {
      if (score[i]) {
        display.display(i * 2, display.digitToSegments(score[i] / 10));
        display.display(i * 2 + 1, display.digitToSegments(score[i] % 10) | display.DOT);
      } else {
        display.display(i * 2, display.MINUS);
        display.display(i * 2 + 1, display.MINUS | display.DOT);
      }
    } else {
      display.display(i * 2, 0);
      display.display(i * 2 + 1, display.DOT);
    }
  }

  sleep_mode();
}
