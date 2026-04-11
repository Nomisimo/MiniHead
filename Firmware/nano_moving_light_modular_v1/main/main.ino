/*
 * =====================================================
 *   NANO MOVING LIGHT — Modular Edition
 *   Hardware : ESP32-C3 Super Mini
 * =====================================================
 * main.ino — never needs to be touched.
 * Add/remove modules in config.h only.
 * =====================================================
 */

#include "core.h"
#include "config.h"


void setup() {
  Serial.begin(115200);
  for (int i = 0; i < MODULE_COUNT; i++) {
    modules[i].setup();
  }
}

void loop() {
  for (int i = 0; i < MODULE_COUNT; i++) {
    modules[i].loop();
  }
}
