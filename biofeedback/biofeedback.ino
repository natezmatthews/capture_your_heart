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

const uint8_t bufferCount = 20;
uint32_t values [bufferCount];

const uint8_t recordingCount = 50;
uint8_t recording [recordingCount];
uint8_t recordingLength = 0;

uint32_t typical_wavelength = 11;
bool wavelengthsAreReady = false;
uint32_t wavelengthA = -1;
uint32_t wavelengthB = -1;
bool changeNextRecordingIsAOnUntouch = false;
bool nextRecordingIsA = true;

uint32_t samplesSoFar = 0;
uint32_t unblockedValue = 0;

const uint8_t wavelengthCount = 5;
uint8_t wavelengthsIndex = 0;
uint32_t wavelengths [wavelengthCount];
bool aboveMiddle = true;
uint32_t lastDipStart = 0;

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


  //Take an average of IR readings at power up
  unblockedValue = 0;
  for (byte x = 0 ; x < 32 ; x++)
  {
    unblockedValue += particleSensor.getIR(); //Read the IR value
  }
  unblockedValue /= 32;

  //Fill wavelengths with a good default val
  for (uint32_t i = 0; i < wavelengthCount; i++) {
    wavelengths[i] = typical_wavelength;
  }

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setDither(BRIGHTNESS < 255);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
}

double normalize(double max, double min, double val) {
  return ((2*val - (max+min)) / (max - min)) * 100;
}

uint8_t to_brightness(uint32_t max, uint32_t min, uint32_t val) {
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
// Remove shift_and_insert, use modulo for something faster
// 255 or 256?

uint8_t brightness_by_index(uint16_t i, uint8_t bri8a, uint8_t bri8b)
{
  uint16_t one_sixth_of_strip = NUM_LEDS / 6;
  uint8_t one_third_brigthness = 255 / 3;
  uint8_t two_thirds_brigthness = 2 * 255 / 3;
  if (i < one_sixth_of_strip) {
    return bri8a;
  } else if ((one_sixth_of_strip * 1) < i && i < (one_sixth_of_strip * 2)) {
    if (one_third_brigthness < bri8a) {
      return bri8a;
    } else {
      return 0;
    }
  } else if ((one_sixth_of_strip * 2) < i && i < (one_sixth_of_strip * 3)) {
    if (two_thirds_brigthness < bri8a) {
      return bri8a;
    } else {
      return 0;
    }
  } else if ((one_sixth_of_strip * 3) < i && i < (one_sixth_of_strip * 4)) {
    return bri8b;
  } else if ((one_sixth_of_strip * 4) < i && i < (one_sixth_of_strip * 5)) {
    if (one_third_brigthness < bri8b) {
      return bri8b;
    } else {
      return 0;
    }
  } else if ((one_sixth_of_strip * 5) < i) {
    if (two_thirds_brigthness < bri8b) {
      return bri8b;
    } else {
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
    leds[pixelnumber] = newcolor;
    
    // nblend( leds[pixelnumber], newcolor, 32);
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

uint8_t sharp_wave(uint32_t wavelength) {
  uint32_t ms = millis() % wavelength;
  uint32_t progress = ((2 * 255 * ms) / wavelength);

  if (ms < (wavelength / 2)) {
    return progress;
  } else {
    return (2 * 255) - progress;
  }
}

void loop()
{
  // INITIAL COLLECTION
  uint32_t currVal = particleSensor.getIR();
  bool isTouched = currVal > unblockedValue * 10;
  values[samplesSoFar % bufferCount] = currVal;
  samplesSoFar++; 
  if (samplesSoFar < bufferCount) {
    return;
  }

  // MAX AND MIN FOR NORMALIZATION
  uint32_t max = currVal;
  uint32_t min = currVal;
  for (uint32_t i = 0; i < bufferCount; i++) {
    if (max < values[i]) {
      max = values[i];
    }
    if (min > values[i]) {
      min = values[i];
    }
  }
  uint32_t middle = (max + min) / 2;

  // WAVELENGTH GATHERING
  uint32_t ms = millis();
  uint32_t msSinceLastDip = ms - lastDipStart;
  if (aboveMiddle && currVal < middle) {
    aboveMiddle = false;
    wavelengths[wavelengthsIndex] = msSinceLastDip;
    wavelengthsIndex++;
    if (wavelengthsIndex == wavelengthCount) {
      wavelengthsIndex = 0;
      wavelengthsAreReady = true;
    }
    lastDipStart = ms;
  } else if (!aboveMiddle && currVal >= middle) {
    aboveMiddle = true;
  }

  // WAVELENGTH AVG AND VARIANCE
  uint32_t wavelengthSum = 0;
  uint32_t wavelengthAvg = typical_wavelength;
  uint32_t wavelengthVariance = 0;
  for (uint32_t i = 0; i < wavelengthCount; i++) {
    wavelengthSum += wavelengths[i];
  }
  wavelengthAvg = wavelengthSum / wavelengthCount;
  for (uint32_t i = 0; i < wavelengthCount; i++) {
    uint32_t error = wavelengths[i] - wavelengthAvg;
    wavelengthVariance += error * error;
  }

  // RECORDING
  if (isTouched && wavelengthsAreReady && wavelengthVariance < 10000) {
    if (recordingLength < recordingCount) {
      recordingLength++;
    }
    if (recordingLength >= recordingCount) {
      changeNextRecordingIsAOnUntouch = true;
      if (nextRecordingIsA) {
        wavelengthA = wavelengthAvg;
      } else {
        wavelengthB = wavelengthAvg;
      }
    }
  } else {
    recordingLength = 0;
  }

  if (changeNextRecordingIsAOnUntouch && !isTouched) {
    nextRecordingIsA = !nextRecordingIsA;
    changeNextRecordingIsAOnUntouch = false;
    recordingLength = 0;
  }

  uint8_t brightnessA = 255;
  uint8_t brightnessB = 255;
  if (wavelengthA != -1) {
    brightnessA = sharp_wave(wavelengthA);
  }
  if (wavelengthB != -1) {
    brightnessB = sharp_wave(wavelengthB);
  }
  if (isTouched) {
    if (nextRecordingIsA) {
      brightnessA = to_brightness(max, min, currVal);
    } else {
      brightnessB = to_brightness(max, min, currVal);
    }
  }

  // fill_solid(leds, NUM_LEDS, CHSV(127, 255, brightness));
  // debug_color(brightnessA, brightnessB);
  pride(brightnessA, brightnessB);
  FastLED.show();
  
  // Serial.print("currVal:");
  // Serial.print(currVal);
  // Serial.print(",unblockedVal:");
  // Serial.print(unblockedValue);
  Serial.print("aboveMiddle:");
  Serial.print(aboveMiddle);
  Serial.print(",wavelengthVariance:");
  Serial.print(wavelengthVariance);
  Serial.print(",wavelengthAvg:");
  Serial.print(wavelengthAvg);
  Serial.print(",wavelengthA:");
  Serial.print(wavelengthA);
  Serial.print(",wavelengthB:");
  Serial.println(wavelengthB);

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
