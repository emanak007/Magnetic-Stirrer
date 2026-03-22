#include <Motoron.h>
#include "SharedTypes.h"

// --- HARDWARE SETUP ---
MotoronI2C mc(16); 

const int encoderPinA = A1; 
const int encoderPinB = A3; 
volatile long encoderPos = 0; 
const int PIN_CLAMP_HOME_SW = 12;

extern struct Settings settings;
extern enum OverallState overall_state;

long clamp_target_count = 0;
int activeCommandedSpeed = 0;
bool clamp_pid_active = false;
bool clamp_homed = false;

// Stall Detection Memory
long lastEncoderPosForStall = 0;
unsigned long lastStallCheckTime = 0;

// --- CONFIGURATION ---
const float ENCODER_CPR = 291.0; 
const float COUNTS_PER_DEGREE = ENCODER_CPR / 360.0;

void InitialiseClampMotor() {
  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);
  pinMode(PIN_CLAMP_HOME_SW, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(encoderPinA), readEncoder, CHANGE);

  Wire.begin();
  mc.reinitialize();
  mc.clearResetFlag();
  mc.setCommandTimeoutMilliseconds(1000); 
  
  mc.setMaxAcceleration(1, 200);
  mc.setMaxDeceleration(1, 300);
  
  clamp_homed = false;
}

void readEncoder() {
  if (digitalRead(encoderPinA) == digitalRead(encoderPinB)) {
    encoderPos--; 
  } else {
    encoderPos++; 
  }
}

void SetClampMotorPower(int power) {
  if (power != activeCommandedSpeed) {
    mc.setSpeed(1, power); 
    activeCommandedSpeed = power;
  }
}

// --- KINEMATICS (CAD MATH) ---

float getActuatorAngle(float targetDiameter) {
  if (targetDiameter > 149.0) targetDiameter = 149.0;
  if (targetDiameter < 29.0) targetDiameter = 29.0;
  return (149.0 - targetDiameter) / 1.6;
}

float getMotorAngleDegrees(float targetDiameter) {
  // Adds 5mm for safety buffer
  float ringAngle = getActuatorAngle(targetDiameter + 5.0); 
  float gearRatio = 82.0 / 9.0; 
  return ringAngle * gearRatio;
}

// --- MOTOR CONTROL ---

void SetClampTarget(uint16_t diameter_mm) {
  if (!clamp_homed) HomeClamp();

  // Calculate target based on CAD math
  float targetDeg = getMotorAngleDegrees((float)diameter_mm);
  clamp_target_count = (long)(targetDeg * COUNTS_PER_DEGREE);

  clamp_pid_active = true;
  lastEncoderPosForStall = encoderPos;
  lastStallCheckTime = millis();
  
  Serial.print(F("[CLAMP] Target: ")); Serial.print(diameter_mm);
  Serial.print(F("mm | Motor Angle: ")); Serial.print(targetDeg);
  Serial.print(F(" | Clicks: ")); Serial.println(clamp_target_count);
}

void UpdateClampPID() {
  if (!clamp_pid_active) return;

  mc.resetCommandTimeout(); 

  long error = clamp_target_count - encoderPos;
  int tolerance = 15; // Tightened tolerance for better grip
  int driveSpeed = 650; 

  if (abs(error) <= tolerance) {
    SetClampMotorPower(0);
    clamp_pid_active = false;
    Serial.println(F("[CLAMP] Position Reached."));
    return;
  } 
  
  SetClampMotorPower((error > 0) ? driveSpeed : -driveSpeed);

  // Stall Protection
  if (millis() - lastStallCheckTime > 500) {
    long pulsesMoved = abs(encoderPos - lastEncoderPosForStall);
    if (abs(activeCommandedSpeed) >= 400 && pulsesMoved <= 3) {
      SetClampMotorPower(0);
      clamp_pid_active = false;
      Serial.println(F("🚨 CLAMP CONTACT/STALL! 🚨"));
      // The main loop will see clamp_pid_active is false and move to STIRRING
    }
    lastEncoderPosForStall = encoderPos;
    lastStallCheckTime = millis();
  }
}

// Fix: Now returns actual degrees for the UI to display
float GetCurrentMotorAngle() {
  return (float)encoderPos / COUNTS_PER_DEGREE;
}

bool IsClampFinished() {
  return !clamp_pid_active; 
}

void HomeClamp() {
  Serial.println(F("[CLAMP] Homing..."));
  SetClampMotorPower(-800); 

  unsigned long stallTimer = millis();
  long lastPos = encoderPos;

  while (digitalRead(PIN_CLAMP_HOME_SW) == HIGH) {
    mc.resetCommandTimeout();
    if (millis() - stallTimer > 500) {
      if (abs(encoderPos - lastPos) < 5) {
        SetClampMotorPower(0);
        Serial.println(F("🚨 HOME STALL 🚨"));
        return;
      }
      lastPos = encoderPos;
      stallTimer = millis();
    }
  }

  SetClampMotorPower(0);
  encoderPos = 0; 
  clamp_homed = true;
  Serial.println(F("[CLAMP] Home Found."));
}