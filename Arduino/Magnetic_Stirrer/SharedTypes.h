
#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <Arduino.h>

struct Inputs {
  int   knob_dir;        // +1 CW, 0 idle, -1 CCW
  int   button_state;    // 0 idle, 1 short press, 2 long press
  float temperature_c;   // LM35 reading (smoothed)
  float mass_g;          // Load cell reading (grams)
  long  clamp_angle;     // Clamp encoder count
};

#endif
