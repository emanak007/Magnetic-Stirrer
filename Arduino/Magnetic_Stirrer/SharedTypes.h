/*
  ======== Shared Types ========
  This header exists because the Arduino IDE auto-generates
  forward declarations for functions and sometimes inserts them
  BEFORE struct definitions. Putting shared types here and
  #including at the top of Magnetic_Stirrer.ino fixes it.
*/

#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

#include <Arduino.h>

// ── Input state (updated every loop cycle) ──
// "All inputs (motor encoding info, button state info,
// temperature reading, etc) are stored in the inputs struct.
// These are updated every cycle."
struct Inputs {
  int   knob_dir;        // +1 CW, 0 idle, -1 CCW
  int   button_state;    // 0 idle, 1 short press, 2 long press
  float temperature_c;   // LM35 reading (smoothed)
  float mass_g;          // Load cell reading (grams)
  long  clamp_angle;     // Clamp encoder count
};

#endif
