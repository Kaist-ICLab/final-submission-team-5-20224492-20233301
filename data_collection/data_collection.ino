#include <TinyMLShield.h>
#include <Arduino_LSM9DS1.h>

unsigned long prevTime = 0; 
const int INTERVAL_MS = 1000.0 / 119; // 119Hz sampling rate
int count = 0; // count number to distinguish odd/even number
bool isDrinking = false; // when 

void setup()
{
  // initialize the Serial 
  Serial.begin(9600);
  while (!Serial);

  // initialize the Shield to use button
  initializeShield();

  // initialize IMU sensor
  if (!IMU.begin()){
    Serial.println("Failed to initialize IMU");
    while (1);
  }
  delay(2000);
}

void loop(){
  bool buttonState = readShieldButton();

  if (buttonState == HIGH) {
    Serial.println("button pressed");
    if (count%2 == 0) {
      // even number =-> start motion
      isDrinking = true;
    } else {
      // odd number -> end motion
      isDrinking = false;
    }
    count++;
  }

  // Write timestamp, IMU data, 
  unsigned long currTime = millis(); 
  static unsigned long startTime = currTime;
  if (currTime - prevTime >= INTERVAL_MS) {
    prevTime = currTime; 
    float accX, accY, accZ, gyrX, gyrY, gyrZ;

    if (IMU.accelerationAvailable()) {
      IMU.readAcceleration(accX, accY, accZ);
    }
    if (IMU.gyroscopeAvailable()){
      IMU.readGyroscope(gyrX, gyrY, gyrZ);
    }
    Serial.print(currTime); 
    Serial.print(","); 
    Serial.print(accX);
    Serial.print(",");
    Serial.print(accY);
    Serial.print(",");
    Serial.print(accZ);
    Serial.print(",");
    Serial.print(gyrX);
    Serial.print(",");
    Serial.print(gyrY);
    Serial.print(",");
    Serial.print(gyrZ);
    Serial.print(",");
    if (isDrinking) {
      Serial.println("halfshot");
    } else {
      Serial.println("default");
    }
  }
}

