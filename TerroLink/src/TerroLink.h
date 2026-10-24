#ifndef TERROLINK_H
#define TERROLINK_H

#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// TerroLink Library v1.1
// Copyright (c) 2026 Omar Terro / FuturisTech
// License: MIT (see sdk/terrolink/LICENSE or TerroLink/LICENSE)
// https://github.com/IOTERRO  ·  https://omarterro.fr
// Public SDK for the TerroLink platform. Token auth only. No MASTER_KEY.
//
// Transport (choose BEFORE including this header, see docs/TLS.md):
//   default                      plain MQTT on 1883  (unchanged behaviour)
//   #define TERROLINK_USE_TLS 1  MQTT over TLS on 8883 (WiFiClientSecure)
//                                -> call terro.setCACert(PEM) before begin()
//                                -> TERROLINK_TLS_BROKER must be a HOSTNAME that
//                                   matches the server certificate (not an IP)
//   #define TERROLINK_TLS_ALLOW_INSECURE   encrypt WITHOUT checking the server
//                                          (testing only: vulnerable to MITM)
// In TLS mode the library never falls back to plaintext on its own.
// ---------------------------------------------------------------------------

#ifndef TERROLINK_BROKER
#define TERROLINK_BROKER "130.110.245.84"
#endif

#ifndef TERROLINK_PORT
#define TERROLINK_PORT 1883
#endif

#ifndef TERROLINK_USE_TLS
#define TERROLINK_USE_TLS 0
#endif

#ifndef TERROLINK_TLS_PORT
#define TERROLINK_TLS_PORT 8883
#endif

// Host used in TLS mode. Must match the certificate name (e.g. "mqtt.example.com").
#ifndef TERROLINK_TLS_BROKER
#define TERROLINK_TLS_BROKER TERROLINK_BROKER
#endif

#if TERROLINK_USE_TLS
#include <WiFiClientSecure.h>
#endif

namespace terrolink_detail {
// True for "130.110.245.84"-style hosts (digits and dots only). A TLS certificate
// is issued for a NAME, so name verification cannot succeed against an IP.
inline bool looksLikeIpv4(const char* host) {
    if (host == nullptr || *host == '\0') return false;
    for (const char* p = host; *p; ++p) {
        if (!((*p >= '0' && *p <= '9') || *p == '.')) return false;
    }
    return true;
}
// An unset ESP32 clock reads ~1970; certificates would look "not yet valid".
inline bool clockLooksValid(time_t now) { return now > 1700000000L; }
}  // namespace terrolink_detail

#ifndef TERROLINK_JSON_BYTES
#define TERROLINK_JSON_BYTES 1024
#endif

class TerroLink {
public:
    TerroLink() : _mqtt(_net), _lastReconnect(0), _lastPublish(0) {}

    /**
     * TLS mode only (TERROLINK_USE_TLS 1): PEM of the CA that signed the broker
     * certificate (e.g. ISRG Root X1 for Let's Encrypt). Call BEFORE begin().
     * The string must stay valid (use a string literal / PROGMEM constant).
     */
    void setCACert(const char* pem) {
        _caCert = pem;
#if !TERROLINK_USE_TLS
        Serial.println("[TerroLink] setCACert() ignored: build without TERROLINK_USE_TLS");
#endif
    }

    /** True when this build talks TLS to the broker. */
    static constexpr bool tlsEnabled() { return TERROLINK_USE_TLS != 0; }

    /**
     * Connect Wi-Fi (captive portal if needed) and prepare MQTT.
     *
     * @param projectSlug  Must match the project name registered in the cloud
     *                     (topic segment). Example: "room_climate"
     * @param deviceAuthToken  MQTT password issued by TerroLink Cloud.
     *                         Paste from the Developer section / secrets.h
     * @param setupApName  SoftAP name when WiFiManager needs credentials
     */
    void begin(const char* projectSlug,
               const char* deviceAuthToken,
               const char* setupApName = "TerroLink_Setup") {
        if (projectSlug == nullptr || deviceAuthToken == nullptr) {
            Serial.println("[TerroLink] begin() requires projectSlug and deviceAuthToken");
            return;
        }

        _project = String(projectSlug);
        _password = String(deviceAuthToken);

        WiFi.setAutoReconnect(true);
        WiFiManager wm;
        if (!wm.autoConnect(setupApName)) {
            Serial.println("[TerroLink] Wi-Fi portal failed. Restarting...");
            delay(3000);
            ESP.restart();
        }

        _deviceId = WiFi.macAddress();
        _deviceId.replace(":", "");
        _username = _deviceId;

        _telemetryTopic = "telemetry/" + _username + "/" + _project + "/" + _deviceId;
        _statusTopic = "status/" + _username + "/" + _project + "/" + _deviceId;

        _mqtt.setBufferSize(TERROLINK_JSON_BYTES);
#if TERROLINK_USE_TLS
        _host = TERROLINK_TLS_BROKER;
        _port = TERROLINK_TLS_PORT;
        _mqtt.setSocketTimeout(15);
        // Certificates have validity dates: the clock must be right before connecting.
        configTime(0, 0, "pool.ntp.org", "time.google.com");
        Serial.printf("[TerroLink] TLS mode -> %s:%d\n", _host, (int)_port);
        if (terrolink_detail::looksLikeIpv4(_host)) {
            Serial.println("[TerroLink] WARNING: TLS broker is an IP address; the certificate "
                           "name check will fail. Set TERROLINK_TLS_BROKER to a hostname.");
        }
#else
        _host = TERROLINK_BROKER;
        _port = TERROLINK_PORT;
        Serial.println("[TerroLink] WARNING: plaintext MQTT (no TLS): the token and data can be "
                       "read on the network. See docs/TLS.md.");
#endif
        _mqtt.setServer(_host, _port);

        _doc.clear();
        _doc["device_id"] = _deviceId;

        Serial.println("[TerroLink] Device ID (MQTT user): " + _deviceId);
        Serial.println("[TerroLink] Telemetry topic: " + _telemetryTopic);
    }

    void loop() {
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.reconnect();
            return;
        }
        if (!_mqtt.connected()) {
            unsigned long now = millis();
            if (now - _lastReconnect > 5000) {
                _lastReconnect = now;
                connectMqtt();
            }
        } else {
            _mqtt.loop();
        }
    }

    bool connected() {
        return WiFi.status() == WL_CONNECTED && _mqtt.connected();
    }

    bool readyToPublish(unsigned long intervalMs) {
        if (!connected()) return false;
        unsigned long now = millis();
        if (now - _lastPublish < intervalMs) return false;
        _lastPublish = now;
        return true;
    }

    void write(const char* key, float value) { _doc[key] = value; }
    void write(const char* key, double value) { _doc[key] = value; }
    void write(const char* key, int value) { _doc[key] = value; }
    void write(const char* key, long value) { _doc[key] = value; }
    void write(const char* key, bool value) { _doc[key] = value; }
    void write(const char* key, const char* value) { _doc[key] = value; }
    void write(const char* key, const String& value) { _doc[key] = value; }

    bool publish() {
        if (!connected()) return false;

        _doc["device_id"] = _deviceId;
        _doc["timestamp"] = millis();

        String payload;
        serializeJson(_doc, payload);
        bool ok = _mqtt.publish(_telemetryTopic.c_str(), payload.c_str());

        _doc.clear();
        _doc["device_id"] = _deviceId;
        return ok;
    }

    String deviceId() const { return _deviceId; }
    String project() const { return _project; }
    String telemetryTopic() const { return _telemetryTopic; }

private:
#if TERROLINK_USE_TLS
    WiFiClientSecure _net;
#else
    WiFiClient _net;
#endif
    PubSubClient _mqtt;
    StaticJsonDocument<TERROLINK_JSON_BYTES> _doc;

    const char* _caCert = nullptr;
    const char* _host = nullptr;
    uint16_t _port = 0;
    String _project;
    String _deviceId;
    String _username;
    String _password;
    String _telemetryTopic;
    String _statusTopic;
    unsigned long _lastReconnect;
    unsigned long _lastPublish;

#if TERROLINK_USE_TLS
    // Returns true when it is safe to attempt a TLS connection now.
    bool prepareTls() {
        if (!terrolink_detail::clockLooksValid(time(nullptr))) {
            Serial.println("[TerroLink] waiting for NTP time (TLS needs a valid clock)...");
            return false;
        }
        if (_caCert != nullptr) {
            _net.setCACert(_caCert);
            return true;
        }
#ifdef TERROLINK_TLS_ALLOW_INSECURE
        _net.setInsecure();
        Serial.println("[TerroLink] WARNING: TLS WITHOUT server verification (insecure mode)");
        return true;
#else
        Serial.println("[TerroLink] TLS needs a CA certificate: call terro.setCACert(pem) "
                       "before begin() (see docs/TLS.md). Not connecting.");
        return false;
#endif
    }
#endif

    void connectMqtt() {
#if TERROLINK_USE_TLS
        if (!prepareTls()) return;
#endif
        Serial.print("[TerroLink] MQTT connecting...");
        String lwt = "{\"status\":\"offline\",\"device_id\":\"" + _deviceId + "\"}";
        if (_mqtt.connect(
                _deviceId.c_str(),
                _username.c_str(),
                _password.c_str(),
                _statusTopic.c_str(),
                0,
                true,
                lwt.c_str())) {
            Serial.println(" ok");
            String online = "{\"status\":\"online\",\"device_id\":\"" + _deviceId + "\"}";
            _mqtt.publish(_statusTopic.c_str(), online.c_str(), true);
        } else {
            Serial.printf(" failed rc=%d\n", _mqtt.state());
#if TERROLINK_USE_TLS
            char err[100];
            int e = _net.lastError(err, sizeof(err));
            if (e != 0) {
                Serial.printf("[TerroLink] TLS error %d: %s (check CA, hostname, clock, port)\n", e, err);
            }
#endif
        }
    }
};

#endif
