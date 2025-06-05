// entrypoint

#include <Arduino.h>

#include "config.h"

#if IS_SERVER
#include "mode_server.cpp"
#else
#include "mode_client.cpp"
#endif

void setup() {
  Serial.begin(9600);

  pinMode(MODE_JUMPER_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  setup_mode();
}

void loop() {
  loop_mode();
}