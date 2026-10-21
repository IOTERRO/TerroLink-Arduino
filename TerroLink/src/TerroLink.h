#ifndef TERROLINK_H
#define TERROLINK_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// TerroLink Library v1.0
// Copyright (c) 2026 Omar Terro / FuturisTech
// License: MIT (see sdk/terrolink/LICENSE or TerroLink/LICENSE)
// https://github.com/IOTERRO  ·  https://omarterro.fr
// Public SDK for the TerroLink platform. Token auth only. No MASTER_KEY.
// ---------------------------------------------------------------------------

#ifndef TERROLINK_BROKER
#define TERROLINK_BROKER "130.110.245.84"
#endif

#ifndef TERROLINK_PORT
#define TERROLINK_PORT 1883
#endif

#ifndef TERROLINK_JSON_BYTES
#define TERROLINK_JSON_BYTES 1024
#endif

class TerroLink {
public:
    TerroLink() : _mqtt(_wifiClient), _lastReconnect(0), _lastPublish(0) {}

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
        _mqtt.setServer(TERROLINK_BROKER, TERROLINK_PORT);

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
    WiFiClient _wifiClient;
    PubSubClient _mqtt;
    StaticJsonDocument<TERROLINK_JSON_BYTES> _doc;

    String _project;
    String _deviceId;
    String _username;
    String _password;
    String _telemetryTopic;
    String _statusTopic;
    unsigned long _lastReconnect;
    unsigned long _lastPublish;

    void connectMqtt() {
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
        }
    }
};

#endif
