/*
MIT License

Copyright (c) 2021 touchgadgetdev@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "Adafruit_TinyUSB.h"
#define BOUNCE_WITH_PROMPT_DETECTION
#include "Bounce2.h"

#define NUM_BUTTONS (6)
#define DEBOUNCE    (2)

#include "Adafruit_NeoPixel.h"
#define LED_PIN   (D19)
#define LED_COUNT (6)
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_KEYBOARD(),
};

Adafruit_USBD_HID usb_hid;

typedef struct Button_t {
  const uint8_t pin;
  const uint8_t key_code;
  const uint8_t rgb_index;
  const uint32_t key_color;
  Bounce debouncer;
} Button_t;

#ifdef ARDUINO_ARCH_RP2040
Button_t Buttons[NUM_BUTTONS] = {
  {D7, HID_KEY_ARROW_LEFT, 4, 0x800080UL},
  {D8, HID_KEY_ARROW_DOWN, 3, 0x0000FFUL},
  {D6, HID_KEY_ESCAPE, 5, 0xFF0000UL},
  {D22, HID_KEY_ENTER, 0, 0x00FF00UL},
  {D20, HID_KEY_ARROW_UP, 2, 0x00FF00UL},
  {D21, HID_KEY_ARROW_RIGHT, 1, 0xFF0000UL},
};
#else
#error Define table for your board
#endif

void setup()
{
  strip.begin();
  strip.show();     // Turn off all LEDs
  strip.setBrightness(255);

  usb_hid.setPollInterval(1); // 1000 Hz poll rate, bInterval=1 (millisecond)
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));

  usb_hid.begin();

  // led pin
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  for (int i = 0; i < NUM_BUTTONS; i++) {
    Button_t *b = &Buttons[i];
    b->debouncer = Bounce();
    b->debouncer.attach(b->pin, INPUT_PULLUP);
    b->debouncer.interval(DEBOUNCE);
    strip.setPixelColor(b->rgb_index, b->key_color);
  }
  strip.show();

  // wait until device mounted
  while( !USBDevice.mounted() ) delay(1);
}

uint8_t Key_Modifiers = 0;
uint8_t Key_HID_Report[6];

void key_press(uint8_t key_code)
{
  if ((HID_KEY_CONTROL_LEFT <= key_code) && (key_code <= HID_KEY_GUI_RIGHT)) {
    uint8_t bit_index = key_code - HID_KEY_CONTROL_LEFT;
    Key_Modifiers |= (1 << bit_index);
  }
  else {
    for (int i = 0; i < sizeof(Key_HID_Report); i++) {
      if (Key_HID_Report[i] == key_code) {
        break;
      }
      else if (Key_HID_Report[i] == 0) {
        Key_HID_Report[i] = key_code;
        break;
      }
    }
  }
}

void key_release(uint8_t key_code)
{
  if ((HID_KEY_CONTROL_LEFT <= key_code) && (key_code <= HID_KEY_GUI_RIGHT)) {
    uint8_t bit_index = key_code - HID_KEY_CONTROL_LEFT;
    Key_Modifiers &= ~(1 << bit_index);
  }
  else {
    // Zero matching key codes
    for (int i = 0; i < sizeof(Key_HID_Report); i++) {
      if (Key_HID_Report[i] == key_code) {
        Key_HID_Report[i] = 0;
      }
    }
    // Squeeze out embedded zeros
    for (int i = 0; i < (sizeof(Key_HID_Report) - 1); i++) {
      if ((Key_HID_Report[i] == 0) && (Key_HID_Report[i + 1] != 0)) {
        Key_HID_Report[i] = Key_HID_Report[i + 1];
        Key_HID_Report[i + 1] = 0;
      }
    }
  }
}

bool Button_changed = false;

void loop()
{
  for (int i = 0; i < NUM_BUTTONS; i++) {
    // Update the Bounce instance
    Button_t *b = &Buttons[i];
    b->debouncer.update();
    // Button fell means button pressed
    if ( b->debouncer.fell() ) {
      key_press(b->key_code);
      strip.setPixelColor(b->rgb_index, 0xFFFFFFUL);
      Button_changed = true;
    }
    else if ( b->debouncer.rose() ) {
      key_release(b->key_code);
      strip.setPixelColor(b->rgb_index, b->key_color);
      Button_changed = true;
    }
  }
  if (Button_changed) {
    strip.show();
    if (usb_hid.ready()) {
      static uint32_t last_millis = 0;
      // Do not send report more than once per millisecond
      if (last_millis != millis()) {
        usb_hid.keyboardReport(0, Key_Modifiers, Key_HID_Report);
        last_millis = millis();
        Button_changed = false;
      }
    }
  }
}
