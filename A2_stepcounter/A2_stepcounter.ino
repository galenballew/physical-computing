
/**
* Prints out the current battery level once per second. On the Huzzah32, the
 * A13 pin is internally tied to measuring the battery level of the LiPoly battery
 * https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/esp32-faq
 * 
 * Based on example from "Program the Internet of Things with Swift for iOS" 
 * by Ahmed Bakir. See: https://bit.ly/2zFzfMT 

 https://github.com/makeabilitylab/arduino/blob/master/ESP32/BatteryLevel/BatteryLevel.ino
 
 This example prints out a LIS3DH accel values to an OLED screen
 * 
 * Adafruit Gfx Library:
 * https://learn.adafruit.com/adafruit-gfx-graphics-library/overview
 * 
 * Adafruit OLED tutorials:
 * https://learn.adafruit.com/monochrome-oled-breakouts
 * 
 * Adafruit LIS3DH tutorial:
 * https://learn.adafruit.com/adafruit-lis3dh-triple-axis-accelerometer-breakout
 * 
 * By Jon E. Froehlich
 * @jonfroehlich
 * http://makeabilitylab.io
 * 
 */

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Used for software SPI
#define LIS3DH_CLK 13
#define LIS3DH_MISO 39
#define LIS3DH_MOSI 14
// Used for hardware & software SPI
#define LIS3DH_CS 21


// software SPI
Adafruit_LIS3DH lis = Adafruit_LIS3DH(LIS3DH_CS, LIS3DH_MOSI, LIS3DH_MISO, LIS3DH_CLK);

const int MAX_ANALOG_VAL = 4095;
const float MAX_BATTERY_VOLTAGE = 4.2; // Max LiPoly voltage of a 3.7 battery is 4.2

const int BUFFER = 50;
int stepCount = 0;
float xavg = 0;
float yavg = 0;
float zavg = 0;

const double weight = 109.0;  // in kilograms
const double height = 177.8; // in centimeters
const double averageStrideLength = 0.415 * height;  // avg stride length calculation in metric  https://www.wikihow.com/Measure-Stride-Length#
const float caloriesPerStep = 1 * weight * averageStrideLength/100000; // calories burned per step assuming 1kcal per kg per km https://macrofactorapp.com/exercise-calorie-calculator/#

void setup() {
  Serial.begin(115200);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  // Clear the buffer
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.setCursor(0, 0);
  display.println("Screen initialized!");
  display.display();
  delay(3000);

  display.println("Initializing accelerometer...");
  if (! lis.begin(0x18)) {   // change this to 0x19 for alternative i2c address
    Serial.println("Couldn't start");
    while (1) yield();
  }
  Serial.println("LIS3DH found!");

  lis.setRange(LIS3DH_RANGE_4_G);   // 2, 4, 8 or 16 G!

  Serial.print("Range = ");
  Serial.print(2 << lis.getRange());
  Serial.println("G");

  //calibrate accelerometer, idea from https://github.com/abhaysbharadwaj/ArduinoPedometer/blob/master/ArduinoPedometer.cpp
  float xs = 0;
  float ys = 0;
  float zs = 0;

  for (int k = 0; k < 100; k++) {
    /* Get a new sensor event */ 
    sensors_event_t event; 
    lis.getEvent(&event);
    float x = event.acceleration.x;
    float y = event.acceleration.y;
    float z = event.acceleration.z;
    xs = xs + x;
    ys = ys + y;
    zs = zs + z;
  }

  xavg = xs / 100;
  yavg = ys / 100;
  zavg = zs / 100; 
}


void loop() {
  Serial.println("Looping...");
  int rawValue = analogRead(A13); //read battery
  float voltageLevel = (rawValue / 4095.0) * 2 * 1.1 * 3.3; // calculate voltage level
  float batteryPercent = voltageLevel / MAX_BATTERY_VOLTAGE * 100;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Battery %: ");
  display.println(batteryPercent);
  display.print("Step Count: ");
  display.println(stepCount);
  display.print("Calories: ");
  display.println(stepCount * caloriesPerStep);
  display.println();
  display.println();
  display.println("Goal Progress: ");

  // this is just an example, you'd probably want to set it to something more like stepsCount / 10,000 proportional to SCREEN WIDTH
  display.fillRect(0, 55, stepCount, SCREEN_HEIGHT, WHITE); //progress bar to 128 steps

  display.display();

  stepCount += getSteps();

  delay(100);
}

int getSteps() {
  bool peak = false;
  float a[BUFFER]={0}; //store acceleration magnitudes
  const float SMOOTHING_FACTOR = 0.35; // Exponential moving average smoothing factor
  float sum = 0;

  for (int k = 0; k < BUFFER; k++) {
    /* Get a new sensor event */ 
    // sensors_event_t event; 
    // lis.getEvent(&event);

    // float x = event.acceleration.x - xavg;
    // float y = event.acceleration.y - yavg;
    // float z = event.acceleration.z - zavg;

    lis.read();

    float x = lis.x - xavg;
    float y = lis.y - yavg;
    float z = lis.z - zavg;
    a[k] = sqrt(x*x + y*y + z*z);
    sum += a[k];
    delay(10);
  }

  float threshold = max(sum / BUFFER * 1.1, 10.0); //set threshold to 1.1 times the average acceleration magnitude

  // Apply exponential moving average (low-pass filter)
  float smoothedValue = a[0];
  for (int k = 1; k < BUFFER; k++) {
    smoothedValue = exponentialMovingAverage(a[k], smoothedValue, SMOOTHING_FACTOR);
    a[k] = smoothedValue;
    Serial.println(a[k]);
  }

  for (int k = 1; k < BUFFER-1; k++) {
    if (a[k] > a[k - 1] && a[k] > a[k + 1] && a[k] > threshold) {
      peak = true;
      Serial.println("Step detected!");
    }
  }
  
  if (peak) {
    return 1;
  }
  else {
    return 0;
  } 
}

float exponentialMovingAverage(float currentValue, float previousAverage, float alpha) {
  return alpha * currentValue + (1 - alpha) * previousAverage;
}
