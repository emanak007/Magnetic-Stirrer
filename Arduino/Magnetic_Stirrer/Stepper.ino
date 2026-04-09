#include "Arduino_LED_Matrix.h" // Not needed for motor, but helps with R4 headers
#include "FspTimer.h"


FspTimer stepper_timer;

const int MICROSTEPS_PER_REV = 1600; 


volatile unsigned long current_step_interval_us = 0;
volatile bool is_stepping = false;

unsigned long ramp_start_time   = 0;
unsigned long ramp_duration_ms  = 0;  
uint16_t ramp_target_rpm        = 0;
bool ramp_active                = false;

extern enum OverallState overall_state;


void stepper_timer_callback(timer_callback_args_t __attribute((unused)) *e) {
  if (is_stepping && current_step_interval_us > 0) {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(2);
    digitalWrite(PIN_STEP, LOW);
  }
}

void InitialiseStepper() {
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  digitalWrite(PIN_STEP, LOW);
  digitalWrite(PIN_DIR, LOW); 
  
  // Configure the Hardware Timer for the Uno R4
  uint8_t type;
  int8_t ch = FspTimer::get_available_timer(type);
  if (ch != -1) {
    stepper_timer.begin(TIMER_MODE_PERIODIC, type, ch, 10000.0, 50.0, stepper_timer_callback, nullptr);
    stepper_timer.setup_overflow_irq();
    stepper_timer.open();
    stepper_timer.start();
  }

  is_stepping = false;
}

unsigned long RpmToStepInterval(uint16_t rpm) {
  if (rpm < 1) return 0;
  return 37500UL / (unsigned long)rpm; 
}

void BeginStirRamp(uint16_t target_rpm, uint16_t duration_ms) {
  ramp_target_rpm  = target_rpm;
  ramp_duration_ms = duration_ms;
  ramp_start_time  = millis();
  ramp_active      = true;
  is_stepping      = true;
  Serial.println(F("[STEPPER] Background Timer Active."));
}

void UpdateStepperRamp() {
  if (overall_state != STIRRING) {
    if (is_stepping) StopStepper();
    return;
  }

  if (!is_stepping) return;

  // Calculate the current RPM based on the ramp
  if (ramp_active) {
    unsigned long elapsed = millis() - ramp_start_time;
    
    if (ramp_duration_ms == 0 || elapsed >= ramp_duration_ms) {
      current_step_interval_us = RpmToStepInterval(ramp_target_rpm);
      ramp_active = false;
    } else {
      uint16_t current_rpm = map(elapsed, 0, ramp_duration_ms, 1, ramp_target_rpm);
      current_step_interval_us = RpmToStepInterval(current_rpm);
    }

    // Update the Hardware Timer frequency
    if (current_step_interval_us > 0) {
      float freq = 1000000.0 / (float)current_step_interval_us;
      stepper_timer.set_frequency(freq);
    }
  }
}

void StopStepper() {
  is_stepping      = false;
  ramp_active      = false;
  current_step_interval_us = 0;
  digitalWrite(PIN_STEP, LOW);
  Serial.println(F("[STEPPER] Timer Paused."));
}