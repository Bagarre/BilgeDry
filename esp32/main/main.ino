/*
 * BilgeDry - ESP32-based Dry Bilge Management System
 *
 * Features:
 * - Web-based configuration and status dashboard
 * - Per-zone settings: name, GPIO pin, expected current, enable/disable
 * - INA260 current sensor integration
 * - Dry run detection based on current draw threshold
 * - Mobile-friendly web interface served via SPIFFS
 * - Configurable run interval in minutes
 * - RESTful JSON API for status and config
 *
 * Developed for onboard pump automation using ESP32
 * Default AP mode: SSID "BilgeDry", Password "KIA2GZ4XX"
 */

#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_INA260.h>

AsyncWebServer server(80);
StaticJsonDocument<2048> configDoc;

#include "config.h"  // Thresholds and constants

int interval = 60;  // Default run frequency in minutes
unsigned long lastCycle = 0;
bool pumpActive = false;
unsigned long pumpStartTime = 0;

Adafruit_INA260 ina260;

// Holds configuration for each zone
struct ZoneConfig {
  String name;
  int pin;
  float amp;
  bool enabled;
  unsigned long lastRun;
  bool dryRunDetected;
};

std::vector<ZoneConfig> zones;

// Load configuration from SPIFFS (config.json)
void loadConfig() {
  File file = SPIFFS.open("/config.json", "r");
  if (file) {
    DeserializationError err = deserializeJson(configDoc, file);
    if (!err) {
      interval = configDoc["interval"] | 60;
      zones.clear();
      if (configDoc.containsKey("zones")) {
        JsonArray arr = configDoc["zones"].as<JsonArray>();
        for (JsonObject z : arr) {
          ZoneConfig zone;
          zone.name = z["name"].as<String>();
          zone.pin = z["pin"] | 0;
          zone.amp = z["amp"] | 0.0;
          zone.enabled = z["enabled"] | true;
          zone.lastRun = 0;
          zone.dryRunDetected = false;
          zones.push_back(zone);
        }
      }
    }
    file.close();
  }
  if (zones.empty()) {
    for (int i = 0; i < 4; i++) {
      zones.push_back({"Zone " + String(i + 1), 0, 0.0, true, 0, false});
    }
  }
}

// Save current config to SPIFFS
void saveConfig() {
  configDoc.clear();
  configDoc["interval"] = interval;
  JsonArray arr = configDoc.createNestedArray("zones");
  for (auto& z : zones) {
    JsonObject o = arr.createNestedObject();
    o["name"] = z.name;
    o["pin"] = z.pin;
    o["amp"] = z.amp;
    o["enabled"] = z.enabled;
  }
  File file = SPIFFS.open("/config.json", "w");
  if (file) {
    serializeJson(configDoc, file);
    file.close();
  }
}

void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);
  EEPROM.begin(512);
  loadConfig();

  // Initialize INA260 current sensor
  if (!ina260.begin()) {
    Serial.println("Failed to find INA260 chip");
  } else {
    Serial.println("INA260 connected");
  }

  // Start Wi-Fi AP
  WiFi.softAP("BilgeDry", "KIA2GZ4XX");

  // Serve JSON status
  server.on("/api/v1/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<1024> statusDoc;
    JsonArray zoneArr = statusDoc.createNestedArray("zones");
    float amps = 0.0;
    if (ina260.begin()) amps = ina260.readCurrent() / 1000.0;
    statusDoc["pumpAmps"] = amps;
    statusDoc["interval"] = interval;

    for (size_t i = 0; i < zones.size(); i++) {
      JsonObject z = zoneArr.createNestedObject();
      z["id"] = i;
      z["name"] = zones[i].name;
      z["status"] = zones[i].enabled ? "Idle" : "Disabled";
      z["lastRun"] = zones[i].lastRun;
      z["pin"] = zones[i].pin;
      z["enabled"] = zones[i].enabled;
      z["dryRun"] = zones[i].dryRunDetected;
    }

    String out;
    serializeJson(statusDoc, out);
    request->send(200, "application/json", out);
  });

  // Return current config
  server.on("/api/v1/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<1024> doc;
    doc["interval"] = interval;
    JsonArray arr = doc.createNestedArray("zones");
    for (auto& z : zones) {
      JsonObject o = arr.createNestedObject();
      o["name"] = z.name;
      o["pin"] = z.pin;
      o["amp"] = z.amp;
      o["enabled"] = z.enabled;
    }
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // Accept new config via POST
  server.on("/api/v1/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<1024> doc;
      DeserializationError err = deserializeJson(doc, data);
      if (err) {
        request->send(400, "application/json", "{\"error\":\"Bad JSON\"}");
        return;
      }
      interval = doc["interval"] | 60;
      zones.clear();
      JsonArray arr = doc["zones"].as<JsonArray>();
      for (JsonObject z : arr) {
        ZoneConfig zone;
        zone.name = z["name"].as<String>();
        zone.pin = z["pin"] | 0;
        zone.amp = z["amp"] | 0.0;
        zone.enabled = z["enabled"] | true;
        zone.lastRun = 0;
        zone.dryRunDetected = false;
        zones.push_back(zone);
      }
      saveConfig();
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

  // Serve web UI
  server.serveStatic("/", SPIFFS, "/webroot/").setDefaultFile("index.html");
  server.begin();
}

// Main loop: cycles zones per interval and performs dry run detection
void loop() {
  if (millis() - lastCycle > interval * 60UL * 1000UL) {
    lastCycle = millis();
    for (auto& zone : zones) {
      if (!zone.enabled) continue;

      digitalWrite(zone.pin, HIGH);  // Activate
      unsigned long start = millis();
      unsigned long belowThresholdTime = 0;

      while (millis() - start < 10000) {
        float measuredCurrent = ina260.readCurrent() / 1000.0; // mA to A
        if (measuredCurrent < zone.amp * DRY_RUN_THRESHOLD_RATIO) {
          belowThresholdTime++;
        }
        delay(1);
      }

      digitalWrite(zone.pin, LOW);
      zone.lastRun = millis();
      zone.dryRunDetected = (belowThresholdTime > 8000);
    }
  }
}
