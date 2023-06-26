#include <Wire.h>
#include "MAX30105.h"

#include <FastLED.h>
#define DATA_PIN    25
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    150
#define BRIGHTNESS  255

CRGB leds[NUM_LEDS];

MAX30105 particleSensor;

const int bufferCount = 20;
double weightNumerator = 1.0;
double weightDenominator = 1.0;
double weights [bufferCount];
double weightsSum = 0;
double values [bufferCount];

const int recordingCount = 50;
double recording [recordingCount];
int recordingLength = 0;

double completeRecordingA [recordingCount];
double completeRecordingB [recordingCount];
double recordingAMax;
double recordingAMin;
double recordingBMax;
double recordingBMin;
bool recordingAComplete = false;
bool recordingBComplete = false;
bool changeNextRecordingIsAOnUntouch = false;
bool nextRecordingIsA = true;
// int endOfRecording;

int samplesSoFar = 0;
long unblockedValue = 0;

const int wavelengthCount = 5;
int wavelengthsSoFar = 0;
int wavelengths [wavelengthCount];
bool aboveMiddle = true;
int lastDipStart = 0;

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
  byte ledBrightness = 0x3F; //Options: 0=Off to 255=50mA
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

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setDither(BRIGHTNESS < 255);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
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

uint8_t to_brightness(double max, double min, double val) {
  return (val - min) * 255 / (max - min);
}

// TODO:
// Flicker rainbow
// Second recording
// Different domes
// Loading animation for good recording
// Sine wave instead of raw heartbeat
// Reimplement cutting off end of recording
// Continuos recording rather than first take
// Back to uint8_t
// Add red reading as well as IR

uint8_t brightness_by_index(uint16_t i, uint8_t bri8a, uint8_t bri8b)
{
  uint16_t one_sixth_of_strip = NUM_LEDS / 6;
  uint8_t one_third_brigthness = 255 / 3;
  uint8_t two_thirds_brigthness = 2 * 255 / 3;
  if (i < one_sixth_of_strip) {
    // Serial.println('z');
    return bri8a;
  } else if ((one_sixth_of_strip * 1) < i && i < (one_sixth_of_strip * 2)) {
    if (one_third_brigthness < bri8a) {
      // Serial.println('a');
      return bri8a;
    } else {
      // Serial.println('b');
      return 0;
    }
  } else if ((one_sixth_of_strip * 2) < i && i < (one_sixth_of_strip * 3)) {
    if (two_thirds_brigthness < bri8a) {
      // Serial.println('c');
      return bri8a;
    } else {
      // Serial.println('d');
      return 0;
    }
  } else if ((one_sixth_of_strip * 3) < i && i < (one_sixth_of_strip * 4)) {
    // Serial.println('e');
    return bri8b;
  } else if ((one_sixth_of_strip * 4) < i && i < (one_sixth_of_strip * 5)) {
    if (one_third_brigthness < bri8b) {
      // Serial.println('f');
      return bri8b;
    } else {
      // Serial.println('g');
      return 0;
    }
  } else if ((one_sixth_of_strip * 5) < i) {
    if (two_thirds_brigthness < bri8b) {
      // Serial.println('h');
      return bri8b;
    } else {
      // Serial.println('i');
      return 0;
    }
  }
}

void pride(uint8_t bri8a, uint8_t bri8b) 
{
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;
 
  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint16_t hue16 = sHue16;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);
  
  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sHue16 += deltams * beatsin88( 400, 5,9);
  
  for( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    
    uint8_t bri8 = brightness_by_index(i, bri8a, bri8b);
    // Serial.println(bri8);
    CRGB newcolor = CHSV( hue8, sat8, bri8);
    
    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS-1) - pixelnumber;
    
    nblend( leds[pixelnumber], newcolor, 64);
  }
}

void debug_color(uint8_t bri8a, uint8_t bri8b) 
{
  for( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    uint8_t bri8 = brightness_by_index(i, bri8a, bri8b);
    // Serial.println(bri8);
    CRGB newcolor = CHSV( 90, 255, bri8);
    
    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS-1) - pixelnumber;
    leds[pixelnumber] = newcolor;
  }
}

void loop()
{
  // INITIAL COLLECTION
  double currVal = double (particleSensor.getIR());
  bool isTouched = currVal > unblockedValue * 10;
  shiftAndInsert(values, bufferCount, currVal);
  samplesSoFar++; 
  if (samplesSoFar < bufferCount) {
    return;
  }

  // MAX AND MIN FOR NORMALIZATION
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

  // WAVELENGTH GATHERING
  if (aboveMiddle && currVal < middle) {
    aboveMiddle = false;
    shiftAndInsert(wavelengths, wavelengthCount, samplesSoFar - lastDipStart);
    wavelengthsSoFar++;
    lastDipStart = samplesSoFar;
  } else if (!aboveMiddle && currVal >= middle) {
    aboveMiddle = true;
  }

  // WAVELENGTH AVG AND VARIANCE
  double wavelengthSum = 0.0;
  double wavelengthAvg = 0.0;
  double wavelengthVariance = 0.0;
  if (wavelengthsSoFar >= wavelengthCount) {
    for (int i = 0; i < wavelengthCount; i++) {
      wavelengthSum += double (wavelengths[i]);
    }
    wavelengthAvg = wavelengthSum / wavelengthCount;
    for (int i = 0; i < wavelengthCount; i++) {
      double error = double (wavelengths[i]) - wavelengthAvg;
      wavelengthVariance += error * error;
    }
  }

  // RECORDING
  if (isTouched && wavelengthVariance < wavelengthCount) {
    int recordingIndex = recordingLength % recordingCount;
    recording[recordingIndex] = currVal;
    recordingLength++;
    if (recordingLength >= recordingCount && recordingIndex == 0) {
      // Save, and get max and min
      changeNextRecordingIsAOnUntouch = true;
      if (nextRecordingIsA) {
        recordingAMax = recording[0];
        recordingAMin = recording[0];
        for (int i = 0; i < recordingCount; i++) {
          completeRecordingA[i] = recording[i];
          if (recordingAMax < recording[i]) {
            recordingAMax = recording[i];
          }
          if (recordingAMin > recording[i]) {
            recordingAMin = recording[i];
          }
        }
        recordingAComplete = true;
      } else {
        recordingBMax = recording[0];
        recordingBMin = recording[0];
        for (int i = 0; i < recordingCount; i++) {
          completeRecordingB[i] = recording[i];
          if (recordingBMax < recording[i]) {
            recordingBMax = recording[i];
          }
          if (recordingBMin > recording[i]) {
            recordingBMin = recording[i];
          }
        }
        recordingBComplete = true;
      }

      // double * completeRecording;
      // completeRecording = nextRecordingIsA ? completeRecordingA : completeRecordingB;
      // double recordingMax = recording[0];
      // double recordingMin = recording[0];
      // for (int i = 0; i < recordingCount; i++) {
      //   completeRecording[i] = recording[i];
      //   if (recordingMax < recording[i]) {
      //     recordingMax = recording[i];
      //   }
      //   if (recordingMin > recording[i]) {
      //     recordingMin = recording[i];
      //   }
      // }

      // if (nextRecordingIsA) {
      //   recordingAComplete = true;
      //   nextRecordingIsA = false;
      //   recordingAMax = recordingMax;
      //   recordingAMin = recordingMin;
      // } else {
      //   recordingBComplete = true;
      //   nextRecordingIsA = true;
      //   recordingBMax = recordingMax;
      //   recordingBMin = recordingMin;
      // }
      // Chop off end to make a smoother wave
      // Divide by wavelength average (but do it in a way that preserves more sig figs)
      // int numWavesInRecording = int ((recordingCount * wavelengthCount) / wavelengthSum);
      // endOfRecording = int ( ( ( double (numWavesInRecording) ) * wavelengthSum ) / wavelengthCount );
    }
  } else {
    recordingLength = 0;
  }

  if (changeNextRecordingIsAOnUntouch && !isTouched) {
    nextRecordingIsA = !nextRecordingIsA;
    changeNextRecordingIsAOnUntouch = false;
    recordingLength = 0;
  }

  int recordingIndex = samplesSoFar % recordingCount;
  uint8_t brightnessA = 255;
  uint8_t brightnessB = 255;
  if (recordingAComplete) {
    brightnessA = to_brightness(recordingAMax, recordingAMin, completeRecordingA[recordingIndex]);
  }
  if (recordingBComplete) {
    brightnessB = to_brightness(recordingBMax, recordingBMin, completeRecordingB[recordingIndex]);
  }
  if (isTouched) {
    if (nextRecordingIsA) {
      brightnessA = to_brightness(max, min, currVal);
    } else {
      brightnessB = to_brightness(max, min, currVal);
    }
  }

  // fill_solid(leds, NUM_LEDS, CHSV(127, 255, brightness));
  // pride(brightnessA, brightnessB);
  debug_color(brightnessA, brightnessB);
  FastLED.show();
  
  // Serial.print("currVal:");
  // Serial.print(currVal);
  // Serial.print(",unblockedVal:");
  // Serial.print(unblockedValue);
  // Serial.print(",wavelengthVariance:");
  // Serial.println(wavelengthVariance);

  // double normalized = normalize(max,min,currVal);
  // Serial.print("actual_value:");
  // Serial.print(normalized);
  // Serial.print(",progress_towards_recording:");
  // int progress_towards_recording = -100 + recordingLength * 4;
  // Serial.print(progress_towards_recording > 100 ? 100 : progress_towards_recording);
  // Serial.print(",recording:");
  // if (isTouched) {
  //   if (aboveMiddle) {
  //     Serial.println(100);
  //   } else {
  //     Serial.println(-100);
  //   }
  // } else {
  //   if (recordingAComplete) {
  //     Serial.println(normalize(recordingAMax,recordingAMin,completeRecordingA[samplesSoFar % recordingCount]));
  //   }
  //   else {
  //     Serial.println(0);
  //   }
  // }
}
