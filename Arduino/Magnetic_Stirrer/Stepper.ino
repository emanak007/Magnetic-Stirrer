// --- CONSTANTS ---
const int MICROSTEPS_PER_REV = 1600; 

// --- STATE VARIABLES ---
unsigned long step_interval_us = 0;
unsigned long last_step_us     = 0;
bool stepper_running           = false;

unsigned long ramp_start_time   = 0;
unsigned long ramp_duration_ms  = 0;  
uint16_t ramp_target_rpm        = 0;
bool ramp_active                = false;

// We need to know the state to prevent menu-spinning
extern enum OverallState overall_state;
void InitialiseStepper() {
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  digitalWrite(PIN_STEP, LOW);
  digitalWrite(PIN_DIR, LOW);
  
  stepper_running = false; // Force off at boot
}

unsigned long RpmToStepInterval(uint16_t rpm) {
  if (rpm < 1) return 0;
  // Math optimization: 60,000,000 / (RPM * 1600)
  return 37500UL / (unsigned long)rpm; 
}

void BeginStirRamp(uint16_t target_rpm, uint16_t duration_ms) {
  ramp_target_rpm  = target_rpm;
  ramp_duration_ms = duration_ms;
  ramp_start_time  = millis();
  ramp_active      = true;
  stepper_running  = true;
  last_step_us     = micros();
  Serial.println(F("[STEPPER] Starting Ramp..."));
}

void UpdateStepperRamp() {
  // 1. HARD LOCK: If we aren't stirring, force the motor to stop!
  if (overall_state != STIRRING) {
    if (stepper_running) StopStepper();
    return;
  }

  if (!stepper_running) return;

  // 2. SAFE RAMP LOGIC
  if (ramp_active) {
    unsigned long elapsed = millis() - ramp_start_time;
    
    // Safety: Prevent Divide-by-Zero if ramp time is 0
    if (ramp_duration_ms == 0 || elapsed >= ramp_duration_ms) {
      step_interval_us = RpmToStepInterval(ramp_target_rpm);
      ramp_active = false;
    } else {
      // Smooth linear ramp
      uint16_t current_rpm = map(elapsed, 0, ramp_duration_ms, 1, ramp_target_rpm);
      step_interval_us = RpmToStepInterval(current_rpm);
    }
  }

  // 3. THE "NO CATCH-UP" STEP GENERATOR
  if (step_interval_us > 0) {
    unsigned long now_us = micros();
    
    if (now_us - last_step_us >= step_interval_us) {
      // We use 'now_us' so it never tries to rapid-fire catch up!
      last_step_us = now_us; 

      digitalWrite(PIN_STEP, HIGH);
      delayMicroseconds(2); // 2us is perfect for EasyDriver
      digitalWrite(PIN_STEP, LOW);
    }
  }
}

void StopStepper() {
  stepper_running  = false;
  ramp_active      = false;
  step_interval_us = 0;
  digitalWrite(PIN_STEP, LOW);
  Serial.println(F("[STEPPER] Stopped."));
}