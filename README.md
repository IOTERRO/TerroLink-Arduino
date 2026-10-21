# TerroLink Arduino Library

ESP32 Arduino library for **TerroLink** (by FuturisTech).

Developers read their own sensors, then publish **named datastreams** to Mosquitto.  
The library handles Wi‑Fi, MQTT, topics, and Last Will.  
**No company master key. No SHA‑256 password derivation.** Auth is a **device token** issued by TerroLink Cloud.

**Author:** Omar Terro · [IOTERRO](https://github.com/IOTERRO) · [omarterro.fr](https://omarterro.fr)  
**License:** **MIT** — see [`LICENSE`](LICENSE).

> TerroLink Cloud (server) is a separate proprietary product. This public repository is **only** the device SDK for client developers.

## Install (Arduino IDE)

1. Clone this repo, then copy the inner **`TerroLink`** folder into your Arduino `libraries` directory, **or** ZIP the `TerroLink` folder and install via *Sketch → Include Library → Add .ZIP Library*.
2. Install dependencies: **PubSubClient**, **ArduinoJson**, **WiFiManager** (ESP32).
3. Copy `TerroLink/examples/TemperatureSensor/secrets.h.example` → `secrets.h` and paste the token from TerroLink Cloud.

## Minimal sketch

```cpp
#include <TerroLink.h>
#include "secrets.h"   // DEVICE_AUTH_TOKEN — never commit this file

TerroLink terro;

void setup() {
  terro.begin("room_climate", DEVICE_AUTH_TOKEN);
}

void loop() {
  terro.loop();
  if (terro.readyToPublish(5000)) {
    terro.write("temperature_c", 23.5);
    terro.publish();
  }
}
```

- MQTT username = board MAC (colons removed), set automatically  
- MQTT password = `DEVICE_AUTH_TOKEN` from TerroLink Cloud  
- Topic = `telemetry/{mac}/{project}/{mac}`

See [`docs/AUTH.md`](docs/AUTH.md) for authentication details.

## Folder layout

```text
TerroLink-Arduino/           ← this repository (public)
  README.md
  LICENSE                    ← MIT
  docs/AUTH.md
  TerroLink/                 ← Arduino library root (copy into libraries/)
    library.properties
    src/TerroLink.h
    src/TerroLink.cpp
    examples/
      TemperatureSensor/
      SakoInverterViaTerroLink/   ← migration example only
```
