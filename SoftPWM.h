/* $Id: SoftPWM.h 129 2011-05-27 20:37:08Z bhagman@roguerobotics.com $

  A Software PWM Library
  
  Written by Brett Hagman
  http://www.roguerobotics.com/
  bhagman@roguerobotics.com

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*************************************************/

#ifndef SOFTPWM_H
#define SOFTPWM_H

#include <stdint.h>

#define SOFTPWM_MAXCHANNELS 20
#define SOFTPWM_PWMDEFAULT 0x00

#define SOFTPWM_NORMAL 0
#define SOFTPWM_INVERTED 1

#define ALL -1

void SoftPWMBegin(uint8_t defaultPolarity = SOFTPWM_NORMAL);
void SoftPWMSet(int8_t pin, uint8_t value, uint8_t hardset = 0);
void SoftPWMSetPercent(int8_t pin, uint8_t percent, uint8_t hardset = 0);
void SoftPWMEnd(int8_t pin);
void SoftPWMSetFadeTime(int8_t pin, uint16_t fadeUpTime, uint16_t fadeDownTime);
void SoftPWMSetPolarity(int8_t pin, uint8_t polarity);

#endif

