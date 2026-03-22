/*
  ======== User Inputs ========
  Rotary Encoder on D2/D3
  PTM Button on D4 (active LOW, INPUT_PULLUP)
  Short press = state 1, Long press (1s hold) = state 2
*/

RotaryEncoder encoder(PIN_ENCODER_A, PIN_ENCODER_B, RotaryEncoder::LatchMode::FOUR3);

unsigned long btn_press_time = 0;
bool          is_holding     = false;
const int     HOLD_DURATION  = 1000; // ms

void InitialiseInputs() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
}

int GetRotaryDir() {
  encoder.tick();
  int direction = (int)encoder.getDirection(); // +1 CW, -1 CCW, 0 none
  return direction;
}

int GetButtonState() {
  int btn_state = digitalRead(PIN_BUTTON);
  static int last_btn_state = HIGH;
  int state = 0;


  if (btn_state == LOW && last_btn_state == HIGH) {
    btn_press_time = millis();
    is_holding = false;
  }

  // Button held down — check for long press
  if (btn_state == LOW) {
    if (millis() - btn_press_time >= HOLD_DURATION && !is_holding) {
      state = 2; // long press
      is_holding = true;
    }
  }

  // Button just released
  if (btn_state == HIGH && last_btn_state == LOW) {
    if (millis() - btn_press_time < HOLD_DURATION) {
      state = 1; // short click
    }
    btn_press_time = 0;
  }

  last_btn_state = btn_state;
  return state;
}

Inputs GetInputs() {
  Inputs values;
  

  values.button_state = GetButtonState();
  values.knob_dir     = GetRotaryDir();
  values.clamp_angle = GetCurrentMotorAngle();


  static unsigned long last_sensor_read = 0;
  static float last_mass = 0.0;
  static float last_temp = 0.0;

  if (millis() - last_sensor_read >= 250) {
    last_mass = GetMassGrams();     
    last_temp = GetTemperature();    
    last_sensor_read = millis();
  }
  

  values.mass_g        = last_mass;
  values.temperature_c = last_temp;

  return values;
}