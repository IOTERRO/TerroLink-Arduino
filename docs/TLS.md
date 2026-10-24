# TerroLink over TLS (encrypted MQTT)

## Why

By default the library talks **plain MQTT on port 1883**. Your device token (the MQTT password), the
board MAC and every reading travel unencrypted. Anyone on the same Wi-Fi or on the network path can
read the token and publish as your device.

TLS mode wraps the same MQTT connection in encryption (the same technology as `https://`) **and**
checks that the server really is your broker.

## Status

| | |
| --- | --- |
| Library | TLS support is available (opt-in) since v1.1.0 |
| Default | **Unchanged: plain 1883.** Existing sketches behave exactly as before |
| Server | The broker must offer MQTT over TLS on **8883**. Until your TerroLink Cloud broker does, keep the default |

In TLS mode the library **never falls back to plaintext by itself**. If it cannot do TLS safely it
does not connect and says why on the serial monitor.

## Turn it on

Two `#define`s **before** `#include <TerroLink.h>`, plus the CA certificate:

```cpp
#define TERROLINK_USE_TLS 1
#define TERROLINK_TLS_BROKER "mqtt.example.com"   // hostname, NOT an IP

#include <TerroLink.h>
#include "secrets.h"
#include "ca_cert.h"                // defines TERROLINK_CA_CERT

TerroLink terro;

void setup() {
  terro.setCACert(TERROLINK_CA_CERT);          // before begin()
  terro.begin("room_climate", DEVICE_AUTH_TOKEN);
}
```

A full sketch is in `examples/TemperatureSensorTLS/`.

| Setting | Default | Meaning |
| --- | --- | --- |
| `TERROLINK_USE_TLS` | `0` | `1` = encrypted MQTT |
| `TERROLINK_TLS_PORT` | `8883` | TLS port |
| `TERROLINK_TLS_BROKER` | = `TERROLINK_BROKER` | Host to connect to. **Must be the name on the certificate** |
| `TERROLINK_TLS_ALLOW_INSECURE` | not set | Testing only: encrypt but do **not** verify the server |

With Arduino IDE put the `#define`s at the very top of the sketch. With PlatformIO you can use
`build_flags = -DTERROLINK_USE_TLS=1 -DTERROLINK_TLS_BROKER=\"mqtt.example.com\"`.

## The three things TLS needs

1. **A hostname, not an IP.** A certificate is issued for a name (e.g. `mqtt.example.com`). The check
   cannot succeed against `130.x.x.x`. The library warns if the host looks like an IP.
2. **The CA certificate.** The ESP32 does not ship with a list of trusted authorities. Give it the one
   that signed your broker's certificate. For Let's Encrypt that is **ISRG Root X1**:
   download it from <https://letsencrypt.org/certificates/>, compare the fingerprint shown on that
   page, and paste the PEM into `ca_cert.h` (copy `ca_cert.h.example`). A CA certificate is public,
   it is fine to commit it. Never paste a private key.
3. **The correct time.** Certificates have validity dates and a freshly booted ESP32 thinks it is
   1970. The library starts NTP in `begin()` and **waits** (retrying every 5 s) until the clock is set
   before it connects. The first connection can take a few seconds longer.

## Insecure mode (testing only)

```cpp
#define TERROLINK_TLS_ALLOW_INSECURE
```

The link is encrypted against someone just listening, but the device does **not** check who it is
talking to, so a man-in-the-middle can still impersonate the server. A warning is printed on every
connect. Do not ship products with it.

Without a CA and without this flag, TLS mode refuses to connect.

## Troubleshooting (serial monitor)

| Message | Meaning / fix |
| --- | --- |
| `waiting for NTP time...` | No internet time yet. Check Wi-Fi has internet access (UDP 123) |
| `TLS needs a CA certificate...` | Call `terro.setCACert(...)` before `begin()` |
| `TLS broker is an IP address` | Use the hostname in `TERROLINK_TLS_BROKER` |
| `failed rc=-2` + `TLS error ...` | Handshake failed: wrong CA, name does not match, expired certificate, port 8883 blocked |
| `failed rc=5` | Connected, but the broker refused the token (check token / that the device was registered on the TLS listener) |

## Memory and compatibility

- TLS uses roughly 40-50 KB of RAM during the handshake. Fine on ESP32, keep your sketch lean.
- Plain and TLS builds are chosen at **compile time**: a TLS build only ever creates a TLS client.
- Existing sketches need no change. Switching to TLS is a deliberate step on each device.
