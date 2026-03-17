NAU7802 loadSensor;

void setupLoad() {
  
  Serial.println("Qwiic Scale Calibrartion");
  Wire1.begin();

  if (loadSensor.begin(Wire1) == false) {
    Serial.println("Scale not detected! Check your Qwiic connection.");
    while(1); //Halts code but allows messages to still arrive in serial
  }
  Serial.println("Scale detected!");
  loadSensor.setSampleRate(NAU7802_SPS_80);

}

float getForce() {
  if (loadSensor.available()) {
    return loadSensor.getWeight();
  }
  return 0.0;
}

float returnZeroOffset() {

  loadSensor.calculateZeroOffset();
  return loadSensor.getZeroOffset();
  
}

void calibrateSensor(float realWeight) {
  loadSensor.calculateCalibrationFactor(realWeight);
  Serial.print("Calibration Factor: ");
  float calFac = loadSensor.getCalibrationFactor();
  loadSensor.setCalibrationFactor(calFac);
  Serial.println(calFac);

}



