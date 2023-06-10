#include <Arduino_LSM9DS1.h>

#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>

#include <ArduinoBLE.h> // for BLE
#include <TinyMLShield.h> // for button input

#include "model.h"

// *************************
// vairable initialization
// *************************

// numSaples: the threshold number of samples for n second window (119Hz)
// in here, 1 second window is used
const int numSamples = 119*1; 

// samplesRead: current number of samples that is read. Inference starts when smaplesRead == numSamples
int samplesRead = 0;

// Necessary variables for using TensorflowLite
tflite::MicroErrorReporter tflErrorReporter;
tflite::AllOpsResolver tflOpsResolver;
const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;
constexpr int tensorArenaSize = 8 * 1024;
byte tensorArena[tensorArenaSize] __attribute__((aligned(16)));
// END: TFLite variable initialization

// Drinking gesture lists
const char* GESTURES[] = {
  "default",
  "halfshot",
  "oneshot"
};
#define NUM_GESTURES (sizeof(GESTURES) / sizeof(GESTURES[0]))

// Current accumulated number of shots
float accNumShots = 0.0;

// Limit number of shots (user input)
float limitNum = 0.0;

// Flag for starting inference (after limit number input stage)
bool isInputDone = false; 

// Time for detecting double click
unsigned long prevInputTime = 0;

// Previuos motion to detect second half shot
int previousMotion = 0; // default


// ********************************************************************************************************
// RunningStatistics class to store 119 samples and return time domain features
// Calculate min, max, mean, and std value when data is pushed
// *******************************************************************************************************
class RunningStatistics {
private:
  int count;
  float mean;
  float variance;
  float minVal;
  float maxVal;

public:
  RunningStatistics() {
    count = 0;
    mean = 0.0;
    variance = 0.0;
    minVal = 0.0;
    maxVal = 0.0;
  }
  // Get new sample and calculate time domain features 
  void push(float value) {
    count++;
    float delta = value - mean;
    mean += delta / count;
    variance += delta * (value - mean);

    if (count == 1) {
      minVal = value;
      maxVal = value;
    } else {
      minVal = min(minVal, value);
      maxVal = max(maxVal, value);
    }
  }
  
  // Return mean value
  float getMean() {
    return mean;
  }
  // Return variance value
  float getVariance() {
    return variance / count;
  }
  // Return std
  float getStandardDeviation() {
    return sqrt(getVariance());
  }
  // Return minimum value
  float getMin(){
    return minVal;
  }
  // Return maximum value
  float getMax(){
    return maxVal;
  }
  // reset the numbers to get new samples in a window.
  void clear() {
    count = 0;
    mean = 0.0;
    variance = 0.0;
    minVal = 0.0;
    maxVal = 0.0;
  }
};
// END: RunningStatistics class

// Initialize RunningStatistics for each raw data
RunningStatistics accXStats, accYStats, accZStats, gyrXStats, gyrYStats, gyrZStats;

// *********************************
// Setup for initializing Arduino
// *********************************
void setup() {
  // =========== BEGIN: Serial communication ============
  Serial.begin(9600);
  while (!Serial);
  // ============ END: Serial communication =============

  // ============ BEGIN: initialize the IMU ===========
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }
  // print out the samples rates of the IMUs
  Serial.print("Accelerometer sample rate = ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println(" Hz");
  Serial.print("Gyroscope sample rate = ");
  Serial.print(IMU.gyroscopeSampleRate());
  Serial.println(" Hz");
  // ============ END: initialize the IMU ==========

  // =======BEGIN: set up TensorFlowLite===============
  // get the TFL representation of the model byte array
  tflModel = tflite::GetModel(model);
  // Create an interpreter to run the model
  tflInterpreter = new tflite::MicroInterpreter(tflModel, tflOpsResolver, tensorArena, tensorArenaSize, &tflErrorReporter);
  // Allocate memory for the model's input and output tensors
  tflInterpreter->AllocateTensors();
  // Get pointers for the model's input and output tensors
  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);
  // ===========END: set up TFlite ===================

  // Initialize button for getting limit number
  initializeShield();
  Serial.println("Please set limit number through button!");
} // end setup()

// ***************************
// main logic for the system 
// ***************************
void loop() {
  // =========== BEGIN: set limit number =============
  if (isInputDone == false) {
    bool buttonState = readShieldButton();
    if (buttonState == HIGH) {
      unsigned long currentTime = millis();
      unsigned long timeDiff = currentTime - prevInputTime;
      prevInputTime = currentTime;
      if (timeDiff < 200) { 
        // double click => limit number setting is done. start inference
        // avg double click time 0.18 sec
          limitNum--;
          Serial.println("===================");
          Serial.print("Limit: ");
          Serial.println(limitNum);
          Serial.println("===================");
          isInputDone = true;
      } else {
        // increase limit number
        limitNum += 1;
        Serial.println("Button clicked");
      }
    }
  }
  // ============= END: set limit number============

  // ============= BEGIN: detecting drinking motion===========
  if (isInputDone) {
    // initialize sensor data variable
    float aX, aY, aZ, gX, gY, gZ;
    
    // ============= BEGIN: infer with model=================
    // When sample is accumulated for 1 window
    if (samplesRead == numSamples) {
      // columns = ['Max-accX', 'Std-accX', 'Max-accY', 'Std-accY', 'Max-accZ', 'Std-accZ', 'Std-gyrX', 'Std-gyrY', 'Min-gyrZ', 'Max-gyrZ', 'Std-gyrZ']
      // The columns are decided during model building process. plz refer to model code for this part
      float maxAccX = accXStats.getMax(); //1
      float stdAccX = accXStats.getStandardDeviation(); //2
      float maxAccY = accYStats.getMax(); //3
      float stdAccY = accYStats.getStandardDeviation(); //4
      float maxAccZ = accZStats.getMax(); //5
      float stdAccZ = accZStats.getStandardDeviation(); //6
      float stdGyrX = accZStats.getStandardDeviation(); //7
      float stdGyrY = gyrYStats.getStandardDeviation(); //8
      float minGyrZ = gyrZStats.getMin(); //9
      float maxGyrZ = gyrZStats.getMax(); // 10
      float stdGyrZ = gyrZStats.getStandardDeviation(); //11

      // input the IMU data to model
      tflInputTensor->data.f[0] = maxAccX;
      tflInputTensor->data.f[1] = stdAccX;
      tflInputTensor->data.f[2] = maxAccY;
      tflInputTensor->data.f[3] = stdAccY;
      tflInputTensor->data.f[4] = maxAccZ;
      tflInputTensor->data.f[5] = stdAccZ;
      tflInputTensor->data.f[6] = stdGyrX;
      tflInputTensor->data.f[7] = stdGyrY;
      tflInputTensor->data.f[8] = minGyrZ;
      tflInputTensor->data.f[9] = maxGyrZ;
      tflInputTensor->data.f[10] = stdGyrZ;
    
      // run detection model
      TfLiteStatus invokeStatus = tflInterpreter->Invoke();
      if (invokeStatus != kTfLiteOk) {
        Serial.println("Invoke failed!");
        while (1);
        return;
      }
      // ========== END: infer with model==============

      // ========== EGIN: check the predicted motion=============
      bool isUpdated = false;
      for (unsigned int i = 0; i < NUM_GESTURES; i++) {
        if ((i == 1) && (tflOutputTensor->data.f[i] > 0.90)){
          // predcited as half shot with accuracy higher than 0.9
          if (previousMotion == 1) {
            // previous Motion is half shot -> detected oneshot is 2nd half shot
            Serial.print("Second half shot detected: ");
            Serial.println(tflOutputTensor->data.f[1], 2);
            previousMotion = 0;
          } else {
            // half shot
            Serial.print("Half shot detected: ");
            Serial.println(tflOutputTensor->data.f[1], 2);
            previousMotion = 1;
          }
          accNumShots += 0.5;
          isUpdated = true;
        } else if ((i==2) && (tflOutputTensor->data.f[i] > 0.90)){
          // predicted as one shot with accuracy higher than 0.9
          if (previousMotion == 1) {
            // previous Motion is half shot -> detected oneshot is 2nd half shot
            accNumShots += 0.5;
            Serial.print("Second half shot detected: ");
            Serial.println(tflOutputTensor->data.f[2], 2);
          } else {
            // one shot
            accNumShots += 1;
            Serial.print("One shot detected: ");
            Serial.println(tflOutputTensor->data.f[2], 2);
          }
          isUpdated = true;
          previousMotion = 2;
        }
      }
      // ============= END: check the predicted model ==================

      // ============= BEGIN: display drinking status for JIT intervention ==================
      if (isUpdated){
        Serial.println(accNumShots);
        // calculating drinking status
        float percentage = accNumShots/limitNum*100;
        Serial.print(percentage, 2);
        Serial.println("%");
        Serial.println("==========");
      }
      // ============ END: display drinking status for JIT intervention ============== 
      
      // ========== BEGIN: reset accumulated data ==============
      samplesRead = 0;
      accXStats.clear();
      accYStats.clear();
      accZStats.clear();
      gyrXStats.clear();
      gyrYStats.clear();
      gyrZStats.clear();
    }
    // ============ END: reset accumulated data =============
  

    // ============== BEGIN: collect IMU data ==============
    while (samplesRead < numSamples) {
      // check if new acceleration AND gyroscope data is available
      if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
        // read the acceleration and gyroscope data
        IMU.readAcceleration(aX, aY, aZ);
        IMU.readGyroscope(gX, gY, gZ);
        accXStats.push(aX);
        accYStats.push(aY);
        accZStats.push(aZ);
        gyrXStats.push(gX);
        gyrYStats.push(gY);
        gyrZStats.push(gZ);
        samplesRead++;
      }
    }
    // ============ END: IMU data ===============
  }
  // ============ END: detect drinking motion ==============
}