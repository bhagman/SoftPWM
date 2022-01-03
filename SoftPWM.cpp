/*
|| @author         Brett Hagman <bhagman@wiring.org.co>
|| @contribution   Paul Stoffregen (paul at pjrc dot com)
|| @url            http://wiring.org.co/
|| @url            http://roguerobotics.com/
||
|| @description
|| | A Software PWM Library
|| | 
|| | Written by Brett Hagman
|| | http://www.roguerobotics.com/
|| | bhagman@roguerobotics.com, bhagman@wiring.org.co
|| |
|| | A Wiring (and Arduino) Library, for Atmel AVR8 bit series microcontrollers,
|| | to produce PWM signals on any arbitrary pin.
|| | 
|| | It was originally designed for controlling the brightness of LEDs, but
|| | could be adapted to control servos and other low frequency PWM controlled
|| | devices as well.
|| | 
|| | It uses a single hardware timer (Timer 2) on the Atmel microcontroller to
|| | generate up to 20 PWM channels (your mileage may vary).
|| | 
|| #
||
|| @license Please see the accompanying LICENSE.txt file for this project.
||
|| @notes
|| | Minor modification by Paul Stoffregen to support different timers.
|| |
|| #
||
|| @name Software PWM Library
|| @type Library
|| @target Atmel AVR 8 Bit
||
|| @version 1.0.1
||
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include "SoftPWM.h"
#include "SoftPWM_timer.h"

#if defined(WIRING)
#include <Wiring.h>
#elif ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#define HARDRESET(RST) if (RST) { SOFTPWM_TIMER_SET(0); _isr_softcount = 0xff; } 

#if F_CPU
#define SOFTPWM_FREQ 60UL
#define SOFTPWM_OCR (F_CPU / (8UL * 256UL * SOFTPWM_FREQ))
#else
// 130 == 60 Hz (on 16 MHz part)
#define SOFTPWM_OCR 130
#endif

volatile uint8_t _isr_softcount = 0xff;
uint8_t _softpwm_defaultPolarity = SOFTPWM_NORMAL;

typedef struct soft_pwm_ch_
{
  // hardware I/O port and pin for this channel
  int8_t pin;
  uint8_t polarity;
  volatile uint8_t *outport;
  uint8_t pinmask;
  uint8_t pwmvalue;
  uint8_t checkval;
  uint8_t fadeuprate;
  uint8_t fadedownrate;
  struct soft_pwm_ch_ *next;
  struct soft_pwm_ch_ *prev;
} softPWMChannel;

softPWMChannel _softpwm_channels[SOFTPWM_MAXCHANNELS];

struct soft_pwm_ch_ *g_used;
struct soft_pwm_ch_ *g_free;

// Here is the meat and gravy
#ifdef WIRING
void SoftPWM_Timer_Interrupt(void)
#else
ISR(SOFTPWM_TIMER_INTERRUPT)
#endif
{
  // uint8_t i;
  int16_t newvalue;
  int16_t direction;
  struct soft_pwm_ch_ *l_it = NULL;
  if (++_isr_softcount == 0)
  {

    l_it = g_used;
    while (l_it)
    {

      if (l_it->fadeuprate > 0 || l_it->fadedownrate > 0)
      {
        // we want to fade to the new value
        direction = l_it->pwmvalue - l_it->checkval;

        // we will default to jumping to the new value
        newvalue = l_it->pwmvalue;

        if (direction > 0 && l_it->fadeuprate > 0)
        {
          newvalue = l_it->checkval + l_it->fadeuprate;
          if (newvalue > l_it->pwmvalue) {
            newvalue = l_it->pwmvalue;
          }
        }
        else if (direction < 0 && l_it->fadedownrate > 0)
        {
          newvalue = l_it->checkval - l_it->fadedownrate;
          if (newvalue < l_it->pwmvalue) {
            newvalue = l_it->pwmvalue;
          }
        }

        l_it->checkval = newvalue;
      }
      else {
        // just set the channel to the new value
        l_it->checkval = l_it->pwmvalue;
      }

      // now set the pin low (checkval == 0x00) otherwise high
      if (l_it->checkval == 0x00)
      {
        if (l_it->polarity == SOFTPWM_NORMAL)
          *(l_it->outport) &= ~(l_it->pinmask);
        else
          *(l_it->outport) |= l_it->pinmask;
      }
      else
      {
        if (l_it->polarity == SOFTPWM_NORMAL)
          *(l_it->outport) |= l_it->pinmask;
        else
          *(l_it->outport) &= ~(l_it->pinmask);
      }

      l_it = l_it->next;
    }
    return;
  }

  l_it = g_used;
  while (l_it) {
    // skip hit on checkval 0 and 255
    if (l_it->checkval != 0x00 && l_it->checkval != 0xFF)
    {
      if (l_it->checkval == _isr_softcount) // if we have hit the width
      {
        // turn off the channel
        if (l_it->polarity == SOFTPWM_NORMAL)
          *(l_it->outport) &= ~(l_it->pinmask);
        else
          *(l_it->outport) |=l_it->pinmask;
      }
    }
    l_it = l_it->next;
  }
}

void ch_push(struct soft_pwm_ch_ *&p_head, struct soft_pwm_ch_ *p_item) {

  p_item->next = p_head;
  p_item->prev = NULL;
  if (NULL == p_head) {
     p_head = p_item;
     return;
  }

  p_head->prev = p_item;
  p_head = p_item;
};
void ch_remove(struct soft_pwm_ch_ *&p_head, struct soft_pwm_ch_ *p_item)
{

  if (NULL == p_head)
  {
    return;
  }

  // remove head
  if (p_head == p_item) {

    p_head = p_item->next;
    if (NULL != p_head) {
      p_head->prev = NULL;
      p_item->next = NULL;
    }

    return;
  }

  struct soft_pwm_ch_ *l_prev = p_item->prev;
  l_prev->next = p_item->next;

  // remove tail
  if (NULL == p_item->next)
  {
    p_item->prev = NULL;
    return;
  }

  // remove middle
  p_item->next->prev = l_prev;

  p_item->prev = NULL;
  p_item->next = NULL;

  return;

};
struct soft_pwm_ch_ *ch_pop(struct soft_pwm_ch_ *&p_head)
{
  struct soft_pwm_ch_ *l_item = NULL;

  if (NULL == p_head)
  {
    return NULL;
  }

  l_item = p_head;
  p_head = l_item->next;
  l_item->next = NULL;

  return l_item;
};
struct soft_pwm_ch_ *ch_find(struct soft_pwm_ch_ *p_head, int8_t p_pin)
{

  uint8_t i = 0;
  struct soft_pwm_ch_ *l_it = p_head;

  for (i = 0; i < SOFTPWM_MAXCHANNELS; i++)
  {
    if (NULL == l_it)
    {
      return NULL;
    }

    if (p_pin == l_it->pin)
    {
      return l_it;
    }

    l_it = l_it->next;
  }

  return NULL;
}

void ch_reset(struct soft_pwm_ch_ *p_ch)
{
    p_ch->pin = -1;
    p_ch->polarity = SOFTPWM_NORMAL;
    p_ch->outport = 0;
    p_ch->fadeuprate = 0;
    p_ch->fadedownrate = 0;
    // l_ch->next = NULL;
    // l_ch->prev = NULL;
}

void SoftPWMBegin(uint8_t defaultPolarity)
{
  // We can tweak the number of PWM period by changing the prescalar
  // and the OCR - we'll default to ck/8 (CS21 set) and OCR=128.
  // This gives 1024 cycles between interrupts.  And the ISR consumes ~200 cycles, so
  // we are looking at about 20 - 30% of CPU time spent in the ISR.
  // At these settings on a 16 MHz part, we will get a PWM period of
  // approximately 60Hz (~16ms).
  
  _softpwm_defaultPolarity = defaultPolarity;

  uint8_t i;
  g_free = NULL;
  g_used = NULL;


  for (i = 0; i < SOFTPWM_MAXCHANNELS; i++)
  {
    struct soft_pwm_ch_ * l_ch = &_softpwm_channels[i];
    ch_reset(l_ch);
    ch_push(g_free, l_ch);
  }


#ifdef WIRING
  Timer2.setMode(0b010); // CTC
  Timer2.setClockSource(CLOCK_PRESCALE_8);
  Timer2.setOCR(CHANNEL_A, SOFTPWM_OCR);
  Timer2.attachInterrupt(INTERRUPT_COMPARE_MATCH_A, SoftPWM_Timer_Interrupt);
#else
  SOFTPWM_TIMER_INIT(SOFTPWM_OCR);
#endif
}

void SoftPWMSetPolarity(int8_t pin, uint8_t polarity)
{
  if (polarity != SOFTPWM_NORMAL) {
    polarity = SOFTPWM_INVERTED;
  }

  struct soft_pwm_ch_ * l_it = NULL;
  // set individual pins
  if (pin > 0) {
    
    l_it = ch_find(g_used, pin);
    if (NULL != l_it) {
      l_it->polarity = polarity;
    }

    return;
  }

  // set all used pins
  l_it = g_used;
  while (l_it) {
    l_it->polarity = polarity;
    l_it = l_it->next;
  }

  return;
}

void SoftPWMSetPercent(int8_t pin, uint8_t percent, uint8_t hardset)
{
  SoftPWMSet(pin, ((uint16_t)percent * 255) / 100, hardset);
}

void SoftPWMSet(int8_t pin, uint8_t value, uint8_t hardset)
{
  struct soft_pwm_ch_* l_it  = NULL;
  // set individual pins
  if (pin > 0) {
    l_it = ch_find(g_used, pin);
    // not found
    if (NULL == l_it) {
      l_it = ch_pop(g_free);
      if (NULL == l_it) {
        // run out of pins
        return;
      }

      l_it->pin = pin;
      l_it->polarity = _softpwm_defaultPolarity;
      l_it->outport = portOutputRegister(digitalPinToPort(pin));
      l_it->pinmask = digitalPinToBitMask(pin);
      l_it->pwmvalue = value;
      l_it->next = NULL;
      l_it->prev = NULL;

      if (l_it->polarity == SOFTPWM_NORMAL) {
        digitalWrite(pin, LOW);
      } else {
        digitalWrite(pin, HIGH);
      }

      pinMode(pin, OUTPUT);

      ch_push(g_used, l_it);
      
      HARDRESET(hardset)

      return;
    }

    // found, just set the value
    l_it->pwmvalue = value;
    HARDRESET(hardset)

    return;
  }

  // set all used pins
  l_it = g_used;
  while (l_it) {
    l_it->pwmvalue = value;
    l_it = l_it->next;
  }
  
  HARDRESET(hardset)

  return;
}

void SoftPWMEnd(int8_t pin)
{

  struct soft_pwm_ch_ *l_ch = ch_find(g_used, pin);
  if (NULL == l_ch)
  {
    // pin not found
    return;
  }

  ch_remove(g_used, l_ch);

  digitalWrite(l_ch->pin, 1);
  pinMode(l_ch->pin, INPUT);

  ch_reset(l_ch);

  ch_push(g_free, l_ch);

}

void SoftPWMSetFadeTime(int8_t pin, uint16_t fadeUpTime, uint16_t fadeDownTime)
{
  int16_t fadeAmount;
  uint8_t i;

  for (i = 0; i < SOFTPWM_MAXCHANNELS; i++)
  {
    if ((pin < 0 && _softpwm_channels[i].pin >= 0) ||  // ALL pins
        (pin >= 0 && _softpwm_channels[i].pin == pin)) // individual pin
    {

      fadeAmount = 0;
      if (fadeUpTime > 0)
        fadeAmount = 255UL * (SOFTPWM_OCR * 256UL / (F_CPU / 8000UL)) / fadeUpTime;

      _softpwm_channels[i].fadeuprate = fadeAmount;

      fadeAmount = 0;
      if (fadeDownTime > 0)
        fadeAmount = 255UL * (SOFTPWM_OCR * 256UL / (F_CPU / 8000UL)) / fadeDownTime;

      _softpwm_channels[i].fadedownrate = fadeAmount;

      if (pin >= 0) // we've set individual pin
        break;
    }
  }
}
