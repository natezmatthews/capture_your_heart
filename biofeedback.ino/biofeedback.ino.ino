#include <Wire.h>
#include "MAX30105.h"
#include "RunningMedian.h"

MAX30105 particleSensor;

const int bufferCount = 20;
double weightNumerator = 1.0;
double weightDenominator = 1.0;
double weights [bufferCount];
double weightsSum = 0;
double values [bufferCount];

const int recordingCount = 50;
double recording [recordingCount];
double completeRecording [recordingCount];
int recordingLength = 0;
bool recordingComplete = false;
int endOfRecording;
double recordingMax;
double recordingMin;

int samplesSoFar = 0;
long unblockedValue = 0;
int x = 1;

double steadyWeightedDeviation = 0.0;

const int wavelengthCount = 5;
int wavelengthsSoFar = 0;
int wavelengths [wavelengthCount];
// RunningMedian runningMedian = RunningMedian(wavelengthCount);
// int waveStage = 0; // 0 - Going up, 1 - Starting top part, 2 - Ending top part, 3 - Going down, 4 - Starting down part, 5 - Ending down part
bool aboveAverage = true;
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
  int pulseWidth = 118; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

  double currWeight = 1;
  for (int i = 0; i < bufferCount; i++) {
    weights[bufferCount - 1 - i] = currWeight;
    weightsSum += currWeight;
    currWeight = currWeight * weightDenominator / weightNumerator;
  }

  //Take an average of IR readings at power up
  unblockedValue = 0;
  for (byte x = 0 ; x < 32 ; x++)
  {
    unblockedValue += particleSensor.getIR(); //Read the IR value
  }
  unblockedValue /= 32;
}

void shiftAndInsert(double array[], int bufferCount, double newValue) {
  for (int i = bufferCount - 1; i > 0; i--) {
    array[i] = array[i - 1];
  }
  array[0] = newValue;
}

void shiftAndInsert(int array[], int bufferCount, double newValue) {
  for (int i = bufferCount - 1; i > 0; i--) {
    array[i] = array[i - 1];
  }
  array[0] = newValue;
}

double normalize(double max, double min, double val) {
  return ((2*val - (max+min)) / (max - min)) * 100;
}

void loop()
{
  double currVal = double (particleSensor.getIR());
  shiftAndInsert(values, bufferCount, currVal);
  samplesSoFar++; 
  if (samplesSoFar < bufferCount) {
    return;
  }

  double max = currVal;
  double min = currVal;
  for (int i = 0; i < bufferCount; i++) {
    if (max < values[i]) {
      max = values[i];
    }
    if (min > values[i]) {
      min = values[i];
    }
  }
  double middle = (max + min) / 2;

  int diff = 0;
  if (aboveAverage && currVal < middle) {
    aboveAverage = false;
    diff = samplesSoFar - lastPeakStart;
    shiftAndInsert(wavelengths, wavelengthCount, diff);
    // runningMedian.add(diff);
    wavelengthsSoFar++;
    lastPeakStart = samplesSoFar;
  } else if (!aboveAverage && currVal >= middle) {
    aboveAverage = true;
  }

  // double weightedSum = 0;
  // for (int i = 0; i < bufferCount; i++) {
  //   weightedSum += values[i] * weights[i];
  // }
  // double weightedAverage = weightedSum / weightsSum;

  // double weightedSumAbsErrors = 0;
  // for (int i = 0; i < bufferCount; i++) {
  //   double error = 0;
  //   if (values[i] > weightedAverage) {
  //     error = values[i] - weightedAverage;
  //   } else {
  //     error = weightedAverage - values[i];
  //   };
  //   weightedSumAbsErrors += error * weights[i];
  // }
  // double weightedDeviation = weightedSumAbsErrors / weightsSum;
  
  // int diff = 0;
  // if (aboveAverage && currVal < weightedAverage) {
  //   aboveAverage = false;
  //   diff = samplesSoFar - lastPeakStart;
  //   shiftAndInsert(wavelengths, wavelengthCount, diff);
  //   runningMedian.add(diff);
  //   wavelengthsSoFar++;
  //   lastPeakStart = samplesSoFar;

  //   steadyWeightedDeviation = weightedDeviation;

  // } else if (!aboveAverage && currVal >= weightedAverage) {
  //   aboveAverage = true;
  // }

  // double threshold = weightedDeviation / 2;
  // int diff = 0;
  // if (waveStage == 0 && currVal > weightedAverage + threshold) {
  //   waveStage = 1;
  //   diff = samplesSoFar - lastPeakStart;
  //   shiftAndInsert(wavelengths, wavelengthCount, diff);
  //   runningMedian.add(diff);
  //   wavelengthsSoFar++;
  //   lastPeakStart = samplesSoFar;

  //   steadyWeightedDeviation = weightedDeviation;

  // } else if (waveStage == 1 && currVal < weightedAverage + threshold) {
  //   waveStage = 2;
  //   diff = samplesSoFar - lastPeakEnd;
  //   shiftAndInsert(wavelengths, wavelengthCount, diff);
  //   runningMedian.add(diff);
  //   wavelengthsSoFar++;
  //   lastPeakEnd = samplesSoFar;
  // } else if (waveStage == 2 && currVal < weightedAverage) {
  //   waveStage = 3;
  //   diff = samplesSoFar - lastMiddle;
  //   shiftAndInsert(wavelengths, wavelengthCount, diff);
  //   runningMedian.add(diff);
  //   wavelengthsSoFar++;
  //   lastMiddle = samplesSoFar;
  // } else if (waveStage == 3 && currVal < weightedAverage - threshold) {
  //   waveStage = 4;
  //   diff = samplesSoFar - lastValleyStart;
  //   shiftAndInsert(wavelengths, wavelengthCount, diff);
  //   runningMedian.add(diff);
  //   wavelengthsSoFar++;
  //   lastValleyStart = samplesSoFar;
  // } else if (waveStage == 4 && currVal > weightedAverage - threshold) {
  //   waveStage = 5;
  //   diff = samplesSoFar - lastValleyEnd;
  //   shiftAndInsert(wavelengths, wavelengthCount, diff);
  //   runningMedian.add(diff);
  //   wavelengthsSoFar++;
  //   lastValleyEnd = samplesSoFar;
  // } else if (waveStage == 5 && currVal > weightedAverage) {
  //   waveStage = 0;
  //   diff = samplesSoFar - lastStart;
  //   shiftAndInsert(wavelengths, wavelengthCount, diff);
  //   runningMedian.add(diff);
  //   wavelengthsSoFar++;
  //   lastStart = samplesSoFar;
  // }

  // long median = runningMedian.getMedian();
  double wavelengthAvg = 0.0;
  double wavelengthVariance = 0.0;
  if (wavelengthsSoFar >= wavelengthCount) {
    double wavelengthSum = 0.0;
    for (int i = 0; i < wavelengthCount; i++) {
      wavelengthSum += double (wavelengths[i]);
    }
    wavelengthAvg = wavelengthSum / wavelengthCount;
    for (int i = 0; i < wavelengthCount; i++) {
      double error = double (wavelengths[i]) - wavelengthAvg;
      wavelengthVariance += error * error;
    }
  }

  double isTouched = currVal - unblockedValue > 50000;

  if (isTouched && wavelengthVariance < wavelengthCount) {
    recording[recordingLength] = currVal;
    recordingLength++;
    if (recordingLength == recordingCount) {
      recordingMax = recording[0];
      recordingMin = recording[0];
      for (int i = 0; i < recordingCount; i++) {
        completeRecording[i] = recording[i];
        if (recordingMax < recording[i]) {
          recordingMax = recording[i];
        }
        if (recordingMin > recording[i]) {
          recordingMin = recording[i];
        }
      }
      recordingComplete = true;
    }
  } else {
    recordingLength = 0;
  }

  // if (wavelengthVariance < wavelengthCount && currVal - unblockedValue > 50000) {
  //   shiftAndInsert(recording, recordingCount, currVal);
  //   recordingLength++;
  // } else {
  //   recordingLength = 0;
  // }
  // if (recordingLength >= bufferCount) {
     
  // }
  
  // if (samplesSoFar % (median / 2) == 0) {
  //   if (x == 1) {
  //     x = -1;
  //   } else {
  //     x = 1;
  //   }
  //   // for (int i = 0; i < median; i++) {
  //   //   Serial.print('*');
  //   // }
  //   // Serial.print('\n');
  // }

  // double normalized = (currVal * weightsSum - weightedSum) / weightedSumAbsErrors;
  // int toDisplay = int (normalized * 100.0);
  
  // Serial.println("-----");
  // Serial.println(currVal);
  // Serial.println(weightedSum / weightsSum);
  // Serial.println(weightedSumAbsErrors / weightsSum);
  // Serial.println(toDisplay);

  // Serial.print("Median: ");
  // Serial.print(median);
  // Serial.print("Normalized:");
  // Serial.print(toDisplay);
  // Serial.print(",Variance:");
  // Serial.println(((wavelengthVariance / wavelengthCount) * steadyWeightedDeviation) / 100);

  // Serial.print("Median:");
  // Serial.print(median),
  // Serial.print(",Curr:");
  // Serial.println(wavelengths[0]);


  // Serial.print("diff:");
  // Serial.print(currVal - unblockedValue);
  double normalized = normalize(max,min,currVal);
  Serial.print("reading:");
  Serial.print(normalized);
  Serial.print(",recording:");
  Serial.print(100 - recordingLength * 4);
  // if (wavelengthVariance < wavelengthCount && currVal - unblockedValue > 50000) {

  // }
  // Serial.print(",variance:");
  // Serial.print(wavelengthVariance / wavelengthCount);
  Serial.print(",wave:");
  if (isTouched) {
    if (aboveAverage) {
      Serial.println(100);
    } else {
      Serial.println(-100);
    }
  } else {
    if (recordingComplete) {
      // Serial.println(200);
      Serial.println(normalize(recordingMax,recordingMin,completeRecording[samplesSoFar % recordingCount]));
    }
    else {
      Serial.println(normalized);
    }
  }
  // Serial.print("wave:");
  // if (currVal - unblockedValue > 50000) {
  //   switch(waveStage) {
  //     case 0:
  //       Serial.println(0.25 * steadyWeightedDeviation);
  //       break;
  //     case 1:
  //       Serial.println(.5 * steadyWeightedDeviation);
  //       break;
  //     case 2:
  //       Serial.println(1.0 * steadyWeightedDeviation);
  //       break;
  //     case 3:
  //       Serial.println(-0.25 * steadyWeightedDeviation);
  //       break;
  //     case 4:
  //       Serial.println(-0.5 * steadyWeightedDeviation);
  //       break;
  //     case 5:
  //       Serial.println(-1.0 * steadyWeightedDeviation);
  //       break;
  //   }
  // } else {
  //   Serial.println(0);
  // }
}
