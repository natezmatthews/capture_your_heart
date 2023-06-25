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
double completeRecording [recordingCount];
int recordingLength = 0;
bool recordingComplete = false;
int endOfRecording;
double recordingMax;
double recordingMin;

int samplesSoFar = 0;
long unblockedValue = 0;

double steadyWeightedDeviation = 0.0;

const int wavelengthCount = 5;
int wavelengthsSoFar = 0;
int wavelengths [wavelengthCount];
bool aboveAverage = true;
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
// Reevaluate cutting off end of recording
// Back to uint8_t

void pride(uint8_t bri8) 
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
    
    CRGB newcolor = CHSV( hue8, sat8, bri8);
    
    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS-1) - pixelnumber;
    
    nblend( leds[pixelnumber], newcolor, 64);
  }
}

void loop()
{
  // INITIAL COLLECTION
  double currVal = double (particleSensor.getIR());
  bool isTouched = currVal > unblockedValue * 100;
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
  if (aboveAverage && currVal < middle) {
    aboveAverage = false;
    shiftAndInsert(wavelengths, wavelengthCount, samplesSoFar - lastDipStart);
    wavelengthsSoFar++;
    lastDipStart = samplesSoFar;
  } else if (!aboveAverage && currVal >= middle) {
    aboveAverage = true;
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
    recording[recordingLength] = currVal;
    if (recordingLength < recordingCount) {
      recordingLength++;
    }
    if (recordingLength == recordingCount) {
      // Save, and get max and min
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
      // Chop off end to make a smoother wave
      // Divide by wavelength average (but do it in a way that preserves more sig figs)
      int numWavesInRecording = int ((recordingCount * wavelengthCount) / wavelengthSum);
      endOfRecording = int ( ( ( double (numWavesInRecording) ) * wavelengthSum ) / wavelengthCount );
    }
  } else {
    recordingLength = 0;
  }

  // uint8_t brightness = (currVal - min) * 255 / (max - min);
  uint8_t brightness = 255;
  if (isTouched) {
    brightness = to_brightness(max, min, currVal);
  } else if (recordingComplete) {
    brightness = to_brightness(recordingMax, recordingMin, completeRecording[samplesSoFar % endOfRecording]);
  }
  // fill_solid(leds, NUM_LEDS, CHSV(127, 255, brightness));
  pride(brightness);
  FastLED.show();
  
  double normalized = normalize(max,min,currVal);
  Serial.print("actual_value:");
  Serial.print(normalized);
  Serial.print(",progress_towards_recording:");
  Serial.print(-100 + recordingLength * 4);
  Serial.print(",recording:");
  if (isTouched) {
    if (aboveAverage) {
      Serial.println(100);
    } else {
      Serial.println(-100);
    }
  } else {
    if (recordingComplete) {
      Serial.println(normalize(recordingMax,recordingMin,completeRecording[samplesSoFar % endOfRecording]));
    }
    else {
      Serial.println(0);
    }
  }
}
