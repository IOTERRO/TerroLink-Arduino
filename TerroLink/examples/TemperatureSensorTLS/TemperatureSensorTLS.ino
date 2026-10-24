/*
  TerroLink v1.1 - temperature client over TLS (encrypted MQTT, port 8883)
  Copyright (c) 2026 Omar Terro / FuturisTech - MIT License
  https://github.com/IOTERRO  -  https://omarterro.fr

  Same as TemperatureSensor, but the token and data are encrypted and the
  server is verified. Read docs/TLS.md first.

  1. Register this ESP32 MAC + project "room_climate" (TerroLink Cloud gives a token).
  2. Copy secrets.h.example -> secrets.h and paste DEVICE_AUTH_TOKEN.
  3. Copy ca_cert.h.example -> ca_cert.h and paste the CA certificate (PEM).
  4. Set TERROLINK_TLS_BROKER below to the HOSTNAME of your broker. It must match
     the server certificate - an IP address will not work.
  5. Flash. The first connection waits a few seconds for the clock (NTP).

  The two #defines must come BEFORE #include <TerroLink.h>.
*/

#define TERROLINK_USE_TLS 1
#define TERROLINK_TLS_BROKER "mqtt.YOURDOMAIN"   // <- change me (hostname, not IP)

#include <TerroLink.h>
#include "secrets.h"
#include "ca_cert.h"   // defines TERROLINK_CA_CERT (PEM string)

TerroLink terro;

float readTemperatureC() {
    // Replace with DHT / DS18B20 / etc.
    return 20.0f + (analogRead(34) / 4095.0f) * 10.0f;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    terro.setCACert(TERROLINK_CA_CERT);          // before begin()
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
