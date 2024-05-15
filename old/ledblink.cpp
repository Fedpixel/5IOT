#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
#define PIN        8
#define NUMPIXELS 16

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500

void setup() {
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif

  pixels.begin();
}

void loop() {
  pixels.clear();
  int i=0;
  pixels.setPixelColor(i, 0, 0, 150);
  pixels.show();
  delay(DELAYVAL);
  pixels.setPixelColor(i, 150, 150, 150);
  pixels.show();
  delay(DELAYVAL);
  pixels.setPixelColor(i, 150, 0, 0);
  pixels.show();
  delay(DELAYVAL);
}