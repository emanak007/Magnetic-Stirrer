#include <Wire.h>
#include "SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h"
#include <U8g2lib.h> //For OLED
#include <SPI.h>
#include <RotaryEncoder.h>

enum {IDLE, SETTINGS, STIRRING}; 
int overall_state = IDLE;

struct Inputs {
  int knobDir;     // 1 clockwise, 0 idle, -1 anticlockwise
  int buttonState; // 1 pressed, 2 held, 0 idle
} inputs;

// Settings Screen Enum
enum SettingsScreen {
    SET_SPEED, SET_TEMP, SET_RAMP, SET_DURATION,
    SET_TEMP_STOP, SET_MASS_STOP, SET_CLAMP, SET_MODE, CONFIRM,
    NUM_SETTINGS 
};

struct SettingsData {
    uint16_t target_rpm;
    uint16_t target_temp;
    uint16_t ramp_time_ms;
    uint32_t stir_duration_ms;
    uint32_t stir_till_temp;
    uint32_t stir_till_mass;
    bool clamp_enabled;
    bool implicit_mode;
} settings;

int highlighted = SET_SPEED; // Start at index 0
bool selected = false;

const char* setting_names[NUM_SETTINGS] = { 
    "SET SPEED", "SET TEMP", "SET RAMP", "SET DURATION",
    "SET TEMP STOP", "SET MASS STOP", "SET CLAMP", "SET MODE", "CONFIRM"
};


void increment_setting(int h) {
  if (h == SET_SPEED) settings.target_rpm += 10;
  else if (h == SET_TEMP) settings.target_temp += 5;
  else if (h == SET_RAMP) settings.ramp_time_ms += 1000;
  else if (h == SET_DURATION) settings.stir_duration_ms += 5000;
  else if (h == SET_TEMP_STOP) settings.stir_till_temp += 5;
  else if (h == SET_MASS_STOP) settings.stir_till_mass += 10;
}

void decrement_setting(int h) {
  if (h == SET_SPEED && settings.target_rpm >= 10) settings.target_rpm -= 10;
  else if (h == SET_TEMP && settings.target_temp >= 5) settings.target_temp -= 5;
  else if (h == SET_RAMP && settings.ramp_time_ms >= 1000) settings.ramp_time_ms -= 1000;
  else if (h == SET_DURATION && settings.stir_duration_ms >= 5000) settings.stir_duration_ms -= 5000;
  else if (h == SET_TEMP_STOP && settings.stir_till_temp >= 5) settings.stir_till_temp -= 5;
  else if (h == SET_MASS_STOP && settings.stir_till_mass >= 10) settings.stir_till_mass -= 10;
}

void toggle_setting(int h) {
  if (h == SET_CLAMP) settings.clamp_enabled = !settings.clamp_enabled;
  else if (h == SET_MODE) settings.implicit_mode = !settings.implicit_mode;
}

void increment_highlighted() { if (highlighted < NUM_SETTINGS - 1) highlighted++; }

void decrement_highlighted() { if (highlighted > 0) highlighted--; }

bool Has_awoken() { return inputs.buttonState == 1 || inputs.buttonState == 2; }

bool Has_started() { return highlighted == CONFIRM && inputs.buttonState == 1; }

void Implicit_stirring_logic() {}

void Explicit_stirring_logic() {}

Inputs GetInputs() {
  Inputs values;
  values.buttonState = GetButtonState(); 
  values.knobDir = GetRotaryDir();       
  return values;
}


void Update_settings() {
    int buttonPressed = inputs.buttonState;
    int knobDir = inputs.knobDir;
    
    
    if (selected) {
        if (knobDir == -1) decrement_setting(highlighted);
        if (knobDir == 1)  increment_setting(highlighted);
    } else {
        if (knobDir == 1)  increment_highlighted(); // Scroll down
        if (knobDir == -1) decrement_highlighted(); // Scroll up
    }

    if (buttonPressed == 1) {
        if (highlighted < SET_CLAMP) { 
            selected = !selected; 
        } else { 
            toggle_setting(highlighted); 
        }
    }
}

// --- 4. ARDUINO SETUP & LOOP ---
void setup() {
  Serial.begin(115200);
  InitialiseOLED();
  InitialiseInputs();
  
  overall_state = IDLE;
}

void loop() {
  inputs = GetInputs();
  //Serial.println(inputs.buttonState);
  switch (overall_state) {
    case IDLE:
      if (Has_awoken()) { overall_state = SETTINGS; }
      break;
      
    case SETTINGS: 
      Update_settings();    
      Display_settings();
      if (Has_started()) { overall_state = STIRRING; }
      break;
      
    case STIRRING:
      if (settings.implicit_mode) {
          Implicit_stirring_logic();
      } else {
          Explicit_stirring_logic();
      }
      break;
  }
}

