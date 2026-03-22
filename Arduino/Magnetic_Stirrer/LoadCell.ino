/*
  ======== Load Cell (NAU7802 Qwiic Scale) ========
  Connected via Qwiic port (Wire1 / I2C bus 1)
  
*/

NAU7802 loadSensor;

void InitialiseLoadCell() {
  Serial.println("Qwiic Scale Init...");
  Wire1.begin();

  if (loadSensor.begin(Wire1) == false) {
    Serial.println("Scale not detected! Check Qwiic connection.");
    return;
  }
  Serial.println("Scale detected!");
  loadSensor.setSampleRate(NAU7802_SPS_80);



  loadSensor.setCalibrationFactor(1118.50); 
  

  loadSensor.calculateZeroOffset(); 
}

// Raw force reading from the load cell
float GetForce() {
  if (loadSensor.available()) {
    return loadSensor.getWeight();
  }
  return 0.0;
}

float GetMassGrams() {
  return GetForce();
}

// Zero the scale (tare)
float returnZeroOffset() {
  loadSensor.calculateZeroOffset();
  return loadSensor.getZeroOffset();
}

// Calibrate with a known weight
void CalibrateLoadCell(float real_weight_g) {
  loadSensor.calculateCalibrationFactor(real_weight_g);
  float cal = loadSensor.getCalibrationFactor();
  loadSensor.setCalibrationFactor(cal);
  Serial.print("Calibration Factor: ");
  Serial.println(cal);
}
