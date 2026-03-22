#pragma once
#include <Adafruit_NeoPixel.h>
static constexpr int BRIGHTNESS = 255;


Adafruit_NeoPixel matrix = Adafruit_NeoPixel(NUMPIXELS, RGB_LED, NEO_GRB + NEO_KHZ800);

void init_rgb(){
  matrix.setBrightness(BRIGHTNESS);
  matrix.begin();
  matrix.show();
}

void set_single_led(uint16_t led_num, uint32_t c)
{
  matrix.setPixelColor(led_num, c);
  matrix.show();
}

void rgb_off(){
  set_single_led(0, matrix.Color(0, 0, 0));
  set_single_led(1, matrix.Color(0, 0, 0));
}

void rgb_color(byte r, byte g, byte b){
  set_single_led(0, matrix.Color(r, g, b));
  set_single_led(1, matrix.Color(r, g, b));
}

