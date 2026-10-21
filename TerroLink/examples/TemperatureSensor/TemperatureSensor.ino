/*
  TerroLink v1 — temperature client example
  Copyright (c) 2026 Omar Terro / FuturisTech — MIT License
  https://github.com/IOTERRO  ·  https://omarterro.fr

  1. Register this ESP32 MAC + project "room_climate" in the TerroLink
     Developer section (TerroLink Cloud generates a device token).
  2. Copy secrets.h.example → secrets.h and paste DEVICE_AUTH_TOKEN.
  3. Flash. Drag a Gauge bound to "temperature_c" in the TerroLink App.
*/

#include <TerroLink.h>
#include "secrets.h"

TerroLink terro;

float readTemperatureC() {
    // Replace with DHT / DS18B20 / etc.
    return 20.0f + (analogRead(34) / 4095.0f) * 10.0f;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    terro.begin("room_climate", DEVICE_AUTH_TOKEN);
    Serial.println(terro.deviceId());
}

void loop() {
    terro.loop();

    if (terro.readyToPublish(5000)) {
        terro.write("temperature_c", readTemperatureC());
        if (terro.publish()) {
            Serial.println("[app] published temperature_c");
        }
    }
}
