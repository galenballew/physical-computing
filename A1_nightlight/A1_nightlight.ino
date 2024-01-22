#include "src/RGBConverter/RGBConverter.h"
#include "NewPing.h"  //ultrasonic sensor
#include <Button.h>   // button
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdint.h>

/*
Virtually all code can be atrributed back to https://makeabilitylab.github.io/physcomp/
and the specific resources it links out to. Any code that is from a different resource
as a specific attribution. 
*/

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


Button button(10);  //class handles debounce for us

const int RGB_RED_PIN = 6;
const int RGB_GREEN_PIN = 5;
const int RGB_BLUE_PIN = 11;

const int PHOTCELL_PIN = A0;
const int MIN_PHOTOCELL_VAL = 960;   // highly dependent on resistors and what kind of lighting environment you're in
const int MAX_PHOTOCELL_VAL = 1025;  // would probably be better with 100k resistor

const int MIC_PIN = A1;
const int MAX_SOUND = 500;  //technically 676 for 3.3v but i found using mappings particular to my env to be better
const int MIN_SOUND = 220;

const int TRIG_PIN = 8;
const int ECHO_PIN = 9;
const int ITERATIONS = 5;
const int MAX_DISTANCE = 10;                      // Maximum distance we want to ping for (in centimeters).
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);  // NewPing setup of pins and maximum distance.
float duration, distance;

const int DELAY_MS = 20;  // delay in ms between changing colors

float _hue = 0;  //hue varies between 0 - 1
float _step = 0.001f;
RGBConverter _rgbConverter;

const int SMOOTHING_WINDOW_SIZE = 10;  // 10 samples
int _samples[SMOOTHING_WINDOW_SIZE];   // the readings from the analog input
int _curReadIndex = 0;                 // the index of the current reading
int _sampleTotal = 0;                  // the running total
int _sampleAvg = 0;                    // the average

enum Mode {
  MODE_CROSSFADE,
  MODE_SOUND,
  MODE_PRESSURE
};

Mode currentMode = MODE_CROSSFADE;

void setup() {
  Serial.begin(9600);
  button.begin();  //software pull-up resistor
  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);
  pinMode(PHOTCELL_PIN, INPUT);
  pinMode(MIC_PIN, INPUT);
  pinMode(TRIG_PIN, INPUT);
  pinMode(ECHO_PIN, INPUT);

  // Initialize the display. If it fails, print failure to Serial
  // and enter an infinite loop
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000);
  display.clearDisplay();
  // display.setCursor(50, 32);
  // display.setTextSize(3);
  // display.print("Mode: ");
  // display.println(currentMode);
  // display.display();
  // delay(2000);

  display.setTextColor(WHITE);
  display.setCursor(24,24);
  display.setTextSize(2);
  display.print("Mode: ");
  display.println(currentMode);
  display.display();
}

void loop() {
  if (button.pressed()) {  //only trigger on press, not release. this prevents us from advancing more than 1 mode at a time
    switchMode();
  }


  switch (currentMode) {
    case MODE_CROSSFADE:
      crossfadeMode();
      break;
    case MODE_SOUND:
      soundMode();
      break;
    case MODE_PRESSURE:
      distanceMode();
      break;
  }
}

void switchMode() {
  currentMode = static_cast<Mode>((currentMode + 1) % 3);
  Serial.print("Switched to Mode: ");
  Serial.println(currentMode);

  display.clearDisplay();
  display.setCursor(24,24);
  display.print("Mode: ");
  display.println(currentMode);
  display.display();
}

void crossfadeMode() {
  int lightLevel = analogRead(PHOTCELL_PIN);
  // Serial.print("Light: ");
  // Serial.println(lightLevel);
  int brightness = map(lightLevel, MIN_PHOTOCELL_VAL, MAX_PHOTOCELL_VAL, 1, 100);  //map to percentages
  brightness = constrain(brightness, 0, 100);
  // Serial.print("Brightness: ");
  // Serial.println(brightness);
  setHue(brightness);
}

void soundMode() {
  int soundLevel = analogRead(MIC_PIN);
  soundLevel = smooth(soundLevel);
  int brightness = map(soundLevel, MIN_SOUND, MAX_SOUND, 1, 100);  //map to percentages
  brightness = constrain(brightness, 0, 100);
  // Serial.print("Brightness: ");
  // Serial.println(brightness);
  setHue(brightness);
}

void distanceMode() {
  delay(50);  // Wait 50ms between pings (about 20 pings/sec). 29ms should be the shortest delay between pings.

  duration = sonar.ping_median(ITERATIONS);
  distance = (duration / 2) * 0.0343;  // centimeters

  int midpoint = MAX_DISTANCE / 2;
  int red = map(distance, midpoint, MAX_DISTANCE, 0, 255);  // far
  int blue = map(distance, midpoint, 0, 0, 255);            // close
  int green = 0;
  if (distance >= 2.5 && distance <= 7.5) {
    float gdist = abs(5 - distance);
    green = map(gdist, 2.5, 0, 0, 255);
  }

  red = constrain(red, 0, 255);
  // Serial.print("Distance: ");
  // Serial.print(distance);
  // Serial.print(" | ");
  // Serial.print("Red: ");
  // Serial.print(red);

  green = constrain(green, 0, 255);
  // Serial.print(" | ");
  // Serial.print("Green: ");
  // Serial.print(green);

  blue = constrain(blue, 0, 255);
  // Serial.print(" | ");
  // Serial.print("Blue: ");
  // Serial.println(blue);
  setColor(red, green, blue, 100);  //max brightness %
}

void setColor(int red, int green, int blue, int brightness) {
  // Serial.print(red);
  // Serial.print(", ");
  // Serial.print(green);
  // Serial.print(", ");
  // Serial.println(blue);

  analogWrite(RGB_RED_PIN, red * brightness / 100);
  analogWrite(RGB_GREEN_PIN, green * brightness / 100);
  analogWrite(RGB_BLUE_PIN, blue * brightness / 100);

  // display.clearDisplay();
  // uint16_t rgb = rgb565(red, green, blue);
  // Serial.println(rgb);
  // display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, rgb);
  // display.display();
}

void setHue(int brightness) {
  byte rgb[3];
  _rgbConverter.hslToRgb(_hue, 1, 0.5, rgb);

  // Set the color
  setColor(rgb[0], rgb[1], rgb[2], brightness);

  // update hue based on step size
  _hue += _step;

  // hue ranges between 0-1, so if > 1, reset to 0
  if (_hue > 1.0) {
    _hue = 0;
  }

  delay(DELAY_MS);
}

int smooth(int input) {
  // subtract the last reading:
  _sampleTotal = _sampleTotal - _samples[_curReadIndex];
  _samples[_curReadIndex] = input;
  // add the reading to the total:
  _sampleTotal = _sampleTotal + _samples[_curReadIndex];
  // advance to the next position in the array:
  _curReadIndex = _curReadIndex + 1;
  // if we're at the end of the array...
  if (_curReadIndex >= SMOOTHING_WINDOW_SIZE) {
    // ...wrap around to the beginning:
    _curReadIndex = 0;
  }
  // calculate the average:
  _sampleAvg = _sampleTotal / SMOOTHING_WINDOW_SIZE;
  return _sampleAvg;
}

// https://forum.arduino.cc/t/8-bit-rgb-value-to-16-bit-colour-value-for-nextion-display/652702/2

//lol just realized the screen is monochrome
uint16_t rgb565(int red, int green, int blue)
{
  uint16_t red_565 = map(red, 0, 255, 0, 31);
  uint16_t green_565 = map(green, 0, 255, 0, 63);
  uint16_t blue_565 = map(blue, 0, 255, 0, 31);
  return (red_565<<11)|(green_565<<5)|blue_565;
}
