/*
  Migration sketch — how Project #1 (Sako) would look on TerroLink v1.
  Copyright (c) 2026 Omar Terro / FuturisTech — MIT License
  https://github.com/IOTERRO  ·  https://omarterro.fr

  - Copy SakoInverter.h from legacy/v0-proof-of-concept/ next to this sketch.
  - This board needs a TOKEN in Mosquitto (not Zero-Touch), so do NOT use
    this on the live Lebanon MAC unless you are on-site to reflash and
    re-provision that device with a token.

  Lebanon production firmware remains under legacy/ (Zero-Touch). Leave it alone.
*/

#include <TerroLink.h>
#include "secrets.h"
#include "SakoInverter.h"

#define INVERTER_RX_PIN 44
#define INVERTER_TX_PIN 43

TerroLink terro;
SakoInverter inverter(&Serial1, true);

void setup() {
    Serial.begin(115200);
    terro.begin("sako_inverter", DEVICE_AUTH_TOKEN, "SmartInverter_Setup");
    inverter.begin(INVERTER_RX_PIN, INVERTER_TX_PIN);
}

void loop() {
    terro.loop();
    if (!terro.readyToPublish(5000)) return;

    InverterData data = inverter.readData();
    terro.write("inverter_status", data.connected ? 1 : 0);
    if (data.connected) {
        terro.write("grid_voltage", data.grid_voltage);
        terro.write("ac_output_voltage", data.ac_output_voltage);
        terro.write("load_watts", data.load_watts);
        terro.write("load_percentage", data.load_percentage);
        terro.write("battery_voltage", data.battery_voltage);
        terro.write("battery_percentage", data.battery_percentage);
        terro.write("temperature_c", data.temperature_c);
        terro.write("solar_watts", data.solar_watts);
        terro.write("solar_voltage", data.solar_voltage);
        terro.write("batt_discharge_amps", data.batt_discharge_amps);
    }
    terro.publish();
}
