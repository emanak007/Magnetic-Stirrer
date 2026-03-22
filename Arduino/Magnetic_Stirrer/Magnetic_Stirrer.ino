/*
  ======== Magnetic Stirrer — Main File ========
  Group 005, COMP0207 Introduction to Electronics, UCL
  Jonah Wilson-Troy, Kaname Asaki, Adeel Imtiazi

  Hardware:
    - Arduino Uno R4 WiFi
    - OLED: SSD1362 256x64 via SPI (CS=10, DC=9, RES=8, MOSI=11, SCK=13)
    - Rotary Encoder: D2 (A), D3 (B)
    - PTM Button: D4 (INPUT_PULLUP, active LOW)
    - Stepper Motor (stirring magnet): EasyDriver on D5 (STEP), D6 (DIR)
    - Status LED: D7
    - LM35 Temperature Sensor: A0 (analogue, 10mV/C)
    - Iris Clamp DC Motor: Motoron M3S550 via I2C (top SDA/SCL)
    - Iris Clamp Encoder: A1 (Phase A), A3 (Phase B)
    - Iris Clamp Homing Switch: TODO — Kaname to assign pin
    - Load Cell (NAU7802 Qwiic Scale): Qwiic port (Wire1)
    - Microphone: REMOVED — not provided by lab

  Coding Guidelines:
    - Non-blocking (no delay())
    - Modular (each subsystem in its own .ino tab)
    - snake_case for variables, PascalCase for functions
    - Collapse-friendly comment indenting

  ======== ======== ======== ======== ========
*/

#include <Wire.h>
#include "SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h"
#include <U8g2lib.h>
#include <SPI.h>
#include <RotaryEncoder.h>
#include <EEPROM.h>
#include "SharedTypes.h"

// ──────────────────────────────────────────────
//  Pin Definitions
// ──────────────────────────────────────────────
  const int PIN_ENCODER_A    = 2;
  const int PIN_ENCODER_B    = 3;
  const int PIN_BUTTON       = 4;
  const int PIN_STEP         = 5;
  const int PIN_DIR          = 6;
  const int PIN_STATUS_LED   = 7;
  const int PIN_OLED_RES     = 8;
  const int PIN_OLED_DC      = 9;
  const int PIN_OLED_CS      = 10;
  // D11 = MOSI (OLED), D13 = SCK (OLED) — used by SPI
  const int PIN_LM35         = A0;
  const int PIN_CLAMP_ENC_A  = A1;
  const int PIN_CLAMP_ENC_B  = A3;
  // const int PIN_CLAMP_HOME_SW = ???; // TODO: Kaname — which pin is the homing limit switch?

// ──────────────────────────────────────────────
//  State Machine
// ──────────────────────────────────────────────
  enum OverallState { IDLE, SETTINGS, CLAMPING, STIRRING, STOPPING };
  OverallState overall_state = IDLE;

// ──────────────────────────────────────────────
//  Stop Reason (Priority 5: show what caused halt)
// ──────────────────────────────────────────────
  enum StopReason {
    STOP_NONE,
    STOP_BUTTON,
    STOP_EMERGENCY,       // long press
    STOP_TEMP,
    STOP_DURATION,
    STOP_MASS,
    STOP_BEAKER_REMOVED,
    STOP_IMPLICIT_REMOVED // implicit mode: user lifted beaker
  };
  StopReason stop_reason = STOP_NONE;

  const char* StopReasonStr(StopReason r) {
    switch (r) {
      case STOP_BUTTON:           return "Button pressed";
      case STOP_EMERGENCY:        return "EMERGENCY STOP";
      case STOP_TEMP:             return "Temp limit hit";
      case STOP_DURATION:         return "Duration elapsed";
      case STOP_MASS:             return "Mass limit hit";
      case STOP_BEAKER_REMOVED:   return "Beaker removed!";
      case STOP_IMPLICIT_REMOVED: return "Beaker lifted";
      default:                    return "Stopped";
    }
  }

// ──────────────────────────────────────────────
//  Inputs (updated every loop cycle)
// ──────────────────────────────────────────────
Inputs inputs;
// ──────────────────────────────────────────────
//  Settings — matches Jonah's latest spec
// ──────────────────────────────────────────────

  // Menu items: numeric settings, toggle settings, and action items
  enum MenuItem {
    MENU_TARGET_RPM,       // uint16_t, increments of 10
    MENU_STOP_TEMP,        // uint16_t C, increments of 5, 0 = OFF
    MENU_RAMP_TIME,        // uint16_t ms, increments of 1000
    MENU_STIR_DURATION,    // uint32_t ms, increments of 5000, 0 = OFF
    MENU_STOP_MASS,        // uint32_t g,  increments of 10,   0 = OFF
    MENU_CLAMP_DIAMETER,   // uint16_t mm, 80-150 in steps of 5, 0 = OFF
    MENU_IMPLICIT_MODE,    // bool toggle
    // --- action items below (no stored value, they run a function) ---
    MENU_ZERO_SCALES,      // action: zeros the load cell
    MENU_SAVE_DEFAULTS,    // action: writes settings to EEPROM
    MENU_START_STIRRING,   // action: transitions to STIRRING state
    NUM_MENU_ITEMS
  };

  // Max limits for settings (Priority 4)
  const uint16_t MAX_RPM           = 2000;
  const uint16_t MAX_STOP_TEMP     = 200;   // C
  const uint16_t MAX_RAMP_TIME_MS  = 30000; // 30s
  const uint32_t MAX_STIR_DURATION = 3600000UL; // 1 hour
  const uint32_t MAX_STOP_MASS     = 5000;  // 5kg
  const uint16_t MAX_CLAMP_DIA     = 150;   // mm
  const uint16_t MIN_CLAMP_DIA     = 80;    // mm
  const uint16_t DEFAULT_IMPLICIT_CLAMP_DIA = 120; // mm — auto-clamp size when implicit mode has clamp OFF

  struct Settings {
    uint16_t target_rpm;        // RPM target for stepper
    uint16_t stop_temp;         // C — stop if reached. 0 = disabled
    uint16_t ramp_time_ms;      // ms — time to reach target_rpm from 0
    uint32_t stir_duration_ms;  // ms — auto-stop after this time. 0 = disabled
    uint32_t stop_mass_g;       // g  — stop when mass reaches this. 0 = disabled
    uint16_t clamp_diameter_mm; // mm — iris target diameter. 0 = clamp OFF
    bool     implicit_mode;     // true = implicit, false = explicit
  } settings;

  // EEPROM magic byte to detect first-ever boot
  const uint8_t EEPROM_MAGIC = 0xA5;
  const int     EEPROM_ADDR_MAGIC    = 0;
  const int     EEPROM_ADDR_SETTINGS = 1;

  int  highlighted = MENU_TARGET_RPM;
  bool selected    = false;  // true = currently editing a numeric value

  // For action-item flash feedback
  unsigned long action_flash_start = 0;
  int           action_flash_item  = -1;
  const unsigned long ACTION_FLASH_DURATION = 300; // ms

  const char* menu_names[NUM_MENU_ITEMS] = {
    "Speed RPM",
    "Stop Temp",
    "Ramp Time",
    "Duration",
    "Stop Mass",
    "Clamp Dia",
    "Mode",
    "Zero Scales",
    "Save Default",
    "START"
  };

// ──────────────────────────────────────────────
//  Stirring state tracking
// ──────────────────────────────────────────────
  unsigned long stir_start_time    = 0;
  float         stir_start_mass    = 0;
  bool          clamp_engaged      = false;

// ──────────────────────────────────────────────
//  Implicit mode — load cell auto-detection
// ──────────────────────────────────────────────
  const float    IMPLICIT_MASS_THRESHOLD  = 50.0;  // grams — beaker detected above this
  const float    IMPLICIT_MASS_HYSTERESIS = 20.0;  // grams — stop below (threshold - hysteresis)
  const unsigned long IMPLICIT_DEBOUNCE_MS = 500;  // sustained for this long before auto-start

  unsigned long implicit_mass_above_since = 0; // millis when mass first exceeded threshold
  bool          implicit_mass_detected    = false;

  bool waiting_for_beaker_removal = false;
// ──────────────────────────────────────────────
//  LED status (Priority 3)
// ──────────────────────────────────────────────
  unsigned long led_last_toggle = 0;
  bool          led_state       = false;

// ──────────────────────────────────────────────
//  STOPPING state timing (Priority 5)
// ──────────────────────────────────────────────
  unsigned long stopping_start_time = 0;
  const unsigned long STOP_REASON_DISPLAY_MS = 2000; // show reason for 2 seconds

// ──────────────────────────────────────────────
//  Timing (non-blocking)
// ──────────────────────────────────────────────
  unsigned long last_loop_time = 0;

// ──────────────────────────────────────────────
//  Settings helpers
// ──────────────────────────────────────────────

  bool IsNumericItem(int idx) {
    return idx >= MENU_TARGET_RPM && idx <= MENU_CLAMP_DIAMETER;
  }

  bool IsToggleItem(int idx) {
    return idx == MENU_IMPLICIT_MODE;
  }

  bool IsActionItem(int idx) {
    return idx >= MENU_ZERO_SCALES && idx <= MENU_START_STIRRING;
  }

  void IncrementSetting(int idx) {
    switch (idx) {
      case MENU_TARGET_RPM:
        if (settings.target_rpm < MAX_RPM) settings.target_rpm += 10;
        break;
      case MENU_STOP_TEMP:
        if (settings.stop_temp < MAX_STOP_TEMP) settings.stop_temp += 5;
        break;
      case MENU_RAMP_TIME:
        if (settings.ramp_time_ms < MAX_RAMP_TIME_MS) settings.ramp_time_ms += 1000;
        break;
      case MENU_STIR_DURATION:
        if (settings.stir_duration_ms < MAX_STIR_DURATION) settings.stir_duration_ms += 5000;
        break;
      case MENU_STOP_MASS:
        if (settings.stop_mass_g < MAX_STOP_MASS) settings.stop_mass_g += 10;
        break;
      case MENU_CLAMP_DIAMETER:
        if (settings.clamp_diameter_mm == 0) settings.clamp_diameter_mm = MIN_CLAMP_DIA;
        else if (settings.clamp_diameter_mm < MAX_CLAMP_DIA) settings.clamp_diameter_mm += 5;
        break;
    }
  }

  void DecrementSetting(int idx) {
    switch (idx) {
      case MENU_TARGET_RPM:
        if (settings.target_rpm >= 10) settings.target_rpm -= 10;
        break;
      case MENU_STOP_TEMP:
        if (settings.stop_temp >= 5) settings.stop_temp -= 5;
        // 0 = OFF
        break;
      case MENU_RAMP_TIME:
        if (settings.ramp_time_ms >= 1000) settings.ramp_time_ms -= 1000;
        break;
      case MENU_STIR_DURATION:
        if (settings.stir_duration_ms >= 5000) settings.stir_duration_ms -= 5000;
        break;
      case MENU_STOP_MASS:
        if (settings.stop_mass_g >= 10) settings.stop_mass_g -= 10;
        break;
      case MENU_CLAMP_DIAMETER:
        if (settings.clamp_diameter_mm > MIN_CLAMP_DIA) settings.clamp_diameter_mm -= 5;
        else settings.clamp_diameter_mm = 0; // wraps to OFF
        break;
    }
  }

  void ToggleSetting(int idx) {
    if (idx == MENU_IMPLICIT_MODE) {
      settings.implicit_mode = !settings.implicit_mode;
    }
  }

  // Returns the display string for a setting's value
  // Writes into the provided buffer
  void GetSettingValueStr(int idx, char* buf, int buf_len) {
    switch (idx) {
      case MENU_TARGET_RPM:
        snprintf(buf, buf_len, "%u", settings.target_rpm);
        break;
      case MENU_STOP_TEMP:
        if (settings.stop_temp == 0) snprintf(buf, buf_len, "OFF");
        else snprintf(buf, buf_len, "%u C", settings.stop_temp);
        break;
      case MENU_RAMP_TIME:
        snprintf(buf, buf_len, "%u s", settings.ramp_time_ms / 1000);
        break;
      case MENU_STIR_DURATION:
        if (settings.stir_duration_ms == 0) snprintf(buf, buf_len, "OFF");
        else snprintf(buf, buf_len, "%lu s", settings.stir_duration_ms / 1000);
        break;
      case MENU_STOP_MASS:
        if (settings.stop_mass_g == 0) snprintf(buf, buf_len, "OFF");
        else snprintf(buf, buf_len, "%lu g", settings.stop_mass_g);
        break;
      case MENU_CLAMP_DIAMETER:
        if (settings.clamp_diameter_mm == 0) snprintf(buf, buf_len, "OFF");
        else snprintf(buf, buf_len, "%u mm", settings.clamp_diameter_mm);
        break;
      case MENU_IMPLICIT_MODE:
        snprintf(buf, buf_len, "%s", settings.implicit_mode ? "AUTO" : "MANUAL");
        break;
      default:
        buf[0] = '\0'; // action items have no value
        break;
    }
  }

// ──────────────────────────────────────────────
//  EEPROM
// ──────────────────────────────────────────────

  void LoadSettingsFromEEPROM() {
    if (EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC) {
      EEPROM.get(EEPROM_ADDR_SETTINGS, settings);
      Serial.println(F("[EEPROM] Settings loaded."));
    } else {
      // First boot — use hardcoded defaults
      settings.target_rpm       = 100;
      settings.stop_temp        = 0;    // OFF
      settings.ramp_time_ms     = 2000;
      settings.stir_duration_ms = 0;    // OFF
      settings.stop_mass_g      = 0;    // OFF
      settings.clamp_diameter_mm= 0;    // OFF
      settings.implicit_mode    = false; // explicit by default
      Serial.println(F("[EEPROM] First boot — using defaults."));
    }
  }

  void SaveSettingsToEEPROM() {
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.put(EEPROM_ADDR_SETTINGS, settings);
    Serial.println(F("[EEPROM] Settings saved."));
  }

// ──────────────────────────────────────────────
//  Update settings (input handling for SETTINGS state)
// ──────────────────────────────────────────────

  void UpdateSettings() {
    int btn  = inputs.button_state;
    int knob = inputs.knob_dir;

    // Long press in SETTINGS returns to IDLE
    if (btn == 2) {
      Serial.println(F("[STATE] SETTINGS -> IDLE (long press)"));
      selected = false;
      overall_state = IDLE;
      return;
    }

    if (selected) {
      // Currently editing a numeric value
      if (knob == 1)  IncrementSetting(highlighted);
      if (knob == -1) DecrementSetting(highlighted);
      if (btn == 1)   selected = false; // confirm edit, deselect
    } else {
      // Navigating menu
      if (knob == 1  && highlighted < NUM_MENU_ITEMS - 1) highlighted++;
      if (knob == -1 && highlighted > 0)                  highlighted--;

      if (btn == 1) {
        if (IsNumericItem(highlighted)) {
          selected = true; // enter edit mode
        } else if (IsToggleItem(highlighted)) {
          ToggleSetting(highlighted);
        } else if (IsActionItem(highlighted)) {
          ExecuteAction(highlighted);
        }
      }
    }
  }

  void ExecuteAction(int idx) {
    switch (idx) {
      case MENU_ZERO_SCALES:
        returnZeroOffset(); 
        action_flash_start = millis();
        action_flash_item  = idx;
        Serial.println(F("[ACTION] Scales zeroed."));
        // Ensure we stay in SETTINGS
        overall_state = SETTINGS; 
        break;

      case MENU_SAVE_DEFAULTS:
        SaveSettingsToEEPROM();
        action_flash_start = millis();
        action_flash_item  = idx;
        // Ensure we stay in SETTINGS
        overall_state = SETTINGS;
        break;

      case MENU_START_STIRRING:
        StartStirring(); // This one SHOULD change the state
        break;
    }
  }

// ──────────────────────────────────────────────
//  LED Status Feedback (Priority 3)
// ──────────────────────────────────────────────

unsigned long error_flash_start  = 0;
  int           error_flash_count  = 0;
  bool          error_flash_active = false;
  const int     ERROR_FLASH_TOTAL  = 6;
  const unsigned long ERROR_FLASH_INTERVAL = 80;

  void TriggerErrorFlash() {
    error_flash_active = true;
    error_flash_count  = 0;
    error_flash_start  = millis();
    led_state = true;
    digitalWrite(PIN_STATUS_LED, HIGH);
  }

  void UpdateStatusLED() {
    unsigned long now = millis();

    // Error flash overrides normal LED behaviour
    if (error_flash_active) {
      if (now - error_flash_start >= ERROR_FLASH_INTERVAL) {
        error_flash_start = now;
        led_state = !led_state;
        digitalWrite(PIN_STATUS_LED, led_state ? HIGH : LOW);
        error_flash_count++;
        if (error_flash_count >= ERROR_FLASH_TOTAL) {
          error_flash_active = false;
          digitalWrite(PIN_STATUS_LED, LOW);
          led_state = false;
        }
      }
      return; // skip normal LED logic while flashing
    }

    switch (overall_state) {
      case IDLE:
        digitalWrite(PIN_STATUS_LED, LOW);
        led_state = false;
        break;

      case SETTINGS:
        digitalWrite(PIN_STATUS_LED, HIGH);
        led_state = true;
        break;

      case STIRRING:
        if (now - led_last_toggle >= 500) {
          led_state = !led_state;
          digitalWrite(PIN_STATUS_LED, led_state ? HIGH : LOW);
          led_last_toggle = now;
        }
        break;

      case STOPPING:
        if (stop_reason == STOP_TEMP || stop_reason == STOP_EMERGENCY
            || stop_reason == STOP_BEAKER_REMOVED) {
          if (now - led_last_toggle >= 100) {
            led_state = !led_state;
            digitalWrite(PIN_STATUS_LED, led_state ? HIGH : LOW);
            led_last_toggle = now;
          }
        } else {
          digitalWrite(PIN_STATUS_LED, HIGH);
        }
        break;
    }
  }

// ──────────────────────────────────────────────
//  Stirring logic
// ──────────────────────────────────────────────

// ──────────────────────────────────────────────
//  Updated StartStirring logic
// ──────────────────────────────────────────────
void StartStirring() {
  if (settings.target_rpm == 0) {
    Serial.println(F("[ERROR] RPM is 0."));
    TriggerErrorFlash(); 
    return;
  }

  // Set the "Before" mass for mass-loss detection
  stir_start_mass = inputs.mass_g;
  stop_reason     = STOP_NONE;

  // CLAMP LOGIC
  if (settings.clamp_diameter_mm > 0 || settings.implicit_mode) {
      uint16_t target_mm = settings.clamp_diameter_mm;
      
      // Implicit mode fallback
      if (settings.implicit_mode && target_mm == 0) {
          target_mm = DEFAULT_IMPLICIT_CLAMP_DIA;
      }

      // This calls the Precision CAD Math we just wrote in the Motor Tab!
      SetClampTarget(target_mm); 
      clamp_engaged = true;
      
      // Go to CLAMPING first. Once IsClampFinished() is true, 
      // the loop will transition us to STIRRING.
      overall_state = CLAMPING; 
      Serial.println(F("[STATE] -> CLAMPING"));
  } else {
      // No clamp needed, jump straight to stirring
      Serial.println(F("[STATE] Clamp OFF -> STIRRING"));
      stir_start_time = millis();
      BeginStirRamp(settings.target_rpm, settings.ramp_time_ms);
      overall_state = STIRRING;
  }
}

void StopStirring(StopReason reason) {
    stop_reason = reason;
    Serial.print(F("[STATE] STIRRING -> STOPPING ("));
    Serial.print(StopReasonStr(reason));
    Serial.println(F(")"));


    if (settings.implicit_mode) {
        waiting_for_beaker_removal = true;
    }
    // ----------------------

    overall_state = STOPPING;
    DisplayStoppingScreen(); 

    // 2. Shut off the magnet
    StopStepper();

    // 3. Retract the clamp (This will block the code, but the screen is already updated!)
    if (clamp_engaged) {
      HomeClamp(); 
      clamp_engaged = false;
    }

    // 4. Start the 2-second timer AFTER the clamp is fully open, 
    // so the message doesn't instantly vanish.
    stopping_start_time = millis(); 
  }
  StopReason CheckStopConditions() {
      // 1. Emergency/Manual Stops
      if (inputs.button_state == 2) return STOP_EMERGENCY;
      if (inputs.button_state == 1) return STOP_BUTTON;
  
      // 2. Temperature Limit
      if (settings.stop_temp > 0) {
        if (inputs.temperature_c >= (float)settings.stop_temp) return STOP_TEMP;
      }
  
      // 3. Time Limit
      if (settings.stir_duration_ms > 0) {
        if (millis() - stir_start_time >= settings.stir_duration_ms) return STOP_DURATION;
      }
  
      // 4. Mass Loss Logic (Stop when mass DECREASES to the target)
      if (settings.stop_mass_g > 0) {
        // Only check if we actually started heavier than our goal
        if (stir_start_mass > (float)settings.stop_mass_g) {
          if (inputs.mass_g <= (float)settings.stop_mass_g) return STOP_MASS;
        }
      }
  
      // 5. Beaker Removed (Only enforced in AUTO/IMPLICIT mode)
      if (settings.implicit_mode && inputs.mass_g < 5.0) return STOP_BEAKER_REMOVED;
  
      return STOP_NONE;
  }

  void ExplicitStirringLogic() {
    // Stepper is already ramping/running from StartStirring()
    UpdateStepperRamp(); // non-blocking ramp progression
    UpdateClampPID();    // keep iris at target

    StopReason reason = CheckStopConditions();
    if (reason != STOP_NONE) {
      StopStirring(reason);
      return;
    }

    DisplayStirringScreen();
  }

  void ImplicitStirringLogic() {
    UpdateStepperRamp();
    UpdateClampPID();

    StopReason reason = CheckStopConditions();
    if (reason != STOP_NONE) {
      StopStirring(reason);
      return;
    }

    DisplayStirringScreen();
  }

// ──────────────────────────────────────────────
//  Implicit mode — IDLE polling (Priority 2)
// ──────────────────────────────────────────────

  void ImplicitIdleCheck() {
    if (settings.target_rpm == 0) return;

    if (inputs.mass_g >= IMPLICIT_MASS_THRESHOLD) {
      // THE INTERLOCK: If the latch is engaged, do absolutely nothing.
      if (waiting_for_beaker_removal) return;

      if (!implicit_mass_detected) {
        implicit_mass_above_since = millis();
        implicit_mass_detected = true;
      } else if (millis() - implicit_mass_above_since >= IMPLICIT_DEBOUNCE_MS) {
        Serial.println(F("[IMPLICIT] Beaker detected — auto-starting."));
        StartStirring();
        implicit_mass_detected = false;
      }
    } else {
      // The mass dropped below the threshold! The beaker was lifted!
      implicit_mass_detected = false;
      
      // Unlock the safety latch so the next beaker can start
      if (waiting_for_beaker_removal) {
          Serial.println(F("[IMPLICIT] Beaker removed. Ready for next cycle."));
          waiting_for_beaker_removal = false;
      }
    }
  }
// ──────────────────────────────────────────────
//  Arduino setup & loop
// ──────────────────────────────────────────────

  void setup() {
    Serial.begin(115200);
    Serial.println(F("=== Magnetic Stirrer v1.0 ==="));
    Serial.println(F("Group 005, COMP0207, UCL"));

    // Load saved settings (or defaults on first boot)
    LoadSettingsFromEEPROM();

    // Initialise subsystems
    InitialiseOLED();
    InitialiseInputs();
    InitialiseLoadCell();
    InitialiseStepper();
    InitialiseClampMotor();
    InitialiseTemperature();

    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    overall_state = IDLE;
    last_loop_time = millis();

    Serial.println(F("[STATE] -> IDLE"));
    Serial.print(F("  Mode: ")); Serial.println(settings.implicit_mode ? F("IMPLICIT") : F("EXPLICIT"));

        // ===== TEMPORARY TEST CODE — REMOVE BEFORE FINAL =====
    //Serial.println(F("[TEST] Saving test settings to EEPROM..."));
    //settings.target_rpm = 500;
    //settings.stop_temp = 45;
    //settings.implicit_mode = true;
    //SaveSettingsToEEPROM();
    //Serial.print(F("[TEST] Saved RPM=")); Serial.println(settings.target_rpm);
    //Serial.print(F("[TEST] Saved stop_temp=")); Serial.println(settings.stop_temp);
    //Serial.print(F("[TEST] Saved implicit=")); Serial.println(settings.implicit_mode);
    // ===== END TEMPORARY TEST CODE =====
  }

  void loop() {
    unsigned long now = millis();
    unsigned long dt  = now - last_loop_time;
    last_loop_time = now;

    inputs = GetInputs();

    switch (overall_state) {
      case IDLE:
        DisplayIdleScreen();

        // Implicit mode: poll load cell for beaker detection
        if (settings.implicit_mode) {
          ImplicitIdleCheck();
        }

        // Any input transitions to SETTINGS (explicit mode, or manual override in implicit)
        if (inputs.button_state == 1 || inputs.button_state == 2 || inputs.knob_dir != 0) {
          Serial.println(F("[STATE] IDLE -> SETTINGS"));
          overall_state = SETTINGS;
        }
        break;

      case SETTINGS:
        UpdateSettings();
        DisplaySettingsScreen();
        // Transition handled by ExecuteAction(MENU_START_STIRRING)
        // or long press returns to IDLE (handled in UpdateSettings)
        break;
      case CLAMPING:
        DisplayStirringScreen(); // Show the UI
        
        if (clamp_engaged) {
            UpdateClampPID(); 
            

            if (IsClampFinished()) { 
                Serial.println(F("[STATE] CLAMPING -> STIRRING"));
                stir_start_time = millis();
                BeginStirRamp(settings.target_rpm, settings.ramp_time_ms);
                overall_state = STIRRING;
            }
        } else {
          
            Serial.println(F("[STATE] CLAMP OFF -> Skipping to STIRRING"));
            BeginStirRamp(settings.target_rpm, settings.ramp_time_ms);
            overall_state = STIRRING;
        }
        break;
        
      case STIRRING:

        static unsigned long last_screen_update = 0;
        if (millis() - last_screen_update >= 500) {
            DisplayStirringScreen(); 
            last_screen_update = millis();
        }
        // -----------------------

        UpdateStepperRamp(); // Give the motor all the CPU speed!

        stop_reason = CheckStopConditions();
        if (stop_reason != STOP_NONE) {
            StopStirring(stop_reason);
        }
        break;

      case STOPPING:
        // Show stop reason briefly, then return to IDLE
        DisplayStoppingScreen();
        UpdateClampPID(); // continue clamp retraction if in progress

        if (now - stopping_start_time >= STOP_REASON_DISPLAY_MS) {
          Serial.println(F("[STATE] STOPPING -> IDLE"));
          stop_reason = STOP_NONE;
          overall_state = IDLE;
        }
        break;
    }

    // LED feedback runs every loop regardless of state
    UpdateStatusLED();
  }
