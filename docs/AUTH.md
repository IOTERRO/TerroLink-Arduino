# TerroLink authentication

## Public library (this package)

| Item | Value |
| --- | --- |
| MQTT username | ESP32 MAC without colons (e.g. `AC276EAF7750`) |
| MQTT password | **Device token** from TerroLink Cloud API |
| In sketch | `terro.begin("project_slug", DEVICE_AUTH_TOKEN)` |

The library does **not** contain `MASTER_KEY` and does **not** hash anything for login.

## Who issues the token?

1. IoT developer opens the **Developer** section of the TerroLink App (future).
2. Enters MAC + project name.
3. TerroLink Cloud generates a **random token**, stores the Mosquitto user, returns the token once.
4. Developer pastes it into `secrets.h`.

Until that UI exists, call `POST /api/developer/register-device` after OTP (or an admin can create the Mosquitto password manually for bench tests) — same string must be pasted into `secrets.h`.

## Legacy Zero‑Touch (not in this library)

Factory / Lebanon Sako boards still use:

```text
password = hex(SHA256(MAC + MASTER_KEY))
```

That code lives only in the local `SakoInverter/legacy/v0-proof-of-concept/` archive (outside this TerroLink repo).  
Existing Mosquitto entries for those MACs must stay unchanged.
