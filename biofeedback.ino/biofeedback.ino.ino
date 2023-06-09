#include <Wire.h>
#include "MAX30105.h"
#include "RunningMedian.h"

MAX30105 particleSensor;

const int bufferCount = 20;
double weightNumerator = 8.0;
double weightDenominator = 10.0;
double weights [bufferCount];
double weightsSum = 0;
double values [bufferCount];

int samplesSoFar = 0;

const int wavelengthCount = 30;
// int wavelengthsSoFar = 0;
// int wavelengths [wavelengthCount];
RunningMedian runningMedian = RunningMedian(wavelengthCount);
int waveStage = 0; // 0 - Going up, 1 - Starting top part, 2 - Ending top part, 3 - Going down, 4 - Starting down part, 5 - Ending down part
int lastStart = 0;
int lastPeakStart = 0;
int lastPeakEnd = 0;
int lastMiddle = 0;
int lastValleyStart = 0;
int lastValleyEnd = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }

  //Setup to sense a nice looking saw tooth on the plotter
  byte ledBrightness = 0x1F; //Options: 0=Off to 255=50mA
  byte sampleAverage = 8; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  int sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

  double currWeight = 1;
  for (int i = 0; i < bufferCount; i++) {
    weights[bufferCount - 1 - i] = currWeight;
    weightsSum += currWeight;
    currWeight = currWeight * weightDenominator / weightNumerator;
  }
}

void shiftAndInsert(double array[], int bufferCount, double newValue) {
  for (int i = bufferCount - 1; i > 0; i--) {
    array[i] = array[i - 1];
  }
  array[0] = newValue;
}

void loop()
{
  double currVal = double (particleSensor.getIR());
  shiftAndInsert(values, bufferCount, currVal);
  samplesSoFar++; 
  if (samplesSoFar < bufferCount) {
    return;
  }

  double weightedSum = 0;
  for (int i = 0; i < bufferCount; i++) {
    weightedSum += values[i] * weights[i];
  }
  double weightedAverage = weightedSum / weightsSum;

  double weightedSumAbsErrors = 0;
  for (int i = 0; i < bufferCount; i++) {
    double error = 0;
    if (values[i] > weightedAverage) {
      error = values[i] - weightedAverage;
    } else {
      error = weightedAverage - values[i];
    };
    weightedSumAbsErrors += error * weights[i];
  }
  double weightedDeviation = weightedSumAbsErrors / weightsSum;

  double threshold = weightedDeviation / 2;
  if (waveStage == 0 && currVal > threshold) {
    waveStage = 1;
    runningMedian.add(samplesSoFar - lastPeakStart)
    shiftAndInsert(wavelengths, wavelengthCount, samplesSoFar - lastPeakStart);
    wavelengthsSoFar++;
    lastPeakStart = samplesSoFar;
  } else if (waveStage == 1 && currVal < threshold) {
    waveStage = 2;
    shiftAndInsert(wavelengths, wavelengthCount, samplesSoFar - lastPeakEnd);
    wavelengthsSoFar++;
    lastPeakEnd = samplesSoFar;
  } else if (waveStage == 2 && currVal < weightedAverage) {
    waveStage = 3;
    shiftAndInsert(wavelengths, wavelengthCount, samplesSoFar - lastMiddle);
    wavelengthsSoFar++;
    lastMiddle = samplesSoFar;
  } else if (waveStage == 3 && currVal < weightedAverage - threshold) {
    waveStage = 4;
    shiftAndInsert(wavelengths, wavelengthCount, samplesSoFar - lastValleyStart);
    wavelengthsSoFar++;
    lastValleyStart = samplesSoFar;
  } else if (waveStage == 4 && currVal > weightedAverage - threshold) {
    waveStage = 5;
    shiftAndInsert(wavelengths, wavelengthCount, samplesSoFar - lastValleyEnd);
    wavelengthsSoFar++;
    lastValleyEnd = samplesSoFar;
  } else if (waveStage == 5 && currVal > weightedAverage) {
    waveStage = 0;
    shiftAndInsert(wavelengths, wavelengthCount, samplesSoFar - lastStart);
    wavelengthsSoFar++;
    lastStart = samplesSoFar;
  }
  if 

  double normalized = (currVal * weightsSum - weightedSum) / weightedSumAbsErrors;
  int toDisplay = int (normalized * 100.0);
  // Serial.println("-----");
  // Serial.println(values[0]);
  // Serial.println(weightedSum / weightsSum);
  // Serial.println(weightedSumAbsErrors / weightsSum);
  Serial.println(toDisplay);
}
