// main.ino (default 4 zones with enable/disable and editable in config UI)
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
StaticJsonDocument<2048> configDoc;

int interval = 60;  // Default run frequency in minutes

struct ZoneConfig {
  String name;
  int pin;
  float amp;
  bool enabled;
};

std::vector<ZoneConfig> zones;

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
          zones.push_back(zone);
        }
      }
    }
    file.close();
  }
  if (zones.empty()) {
    // Default 4 zones
    for (int i = 0; i < 4; i++) {
      zones.push_back({"Zone " + String(i + 1), 0, 0.0, true});
    }
  }
}

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
  loadConfig();

  WiFi.softAP("BilgeDry", "KIA2GZ4");

  server.on("/api/v1/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<1024> statusDoc;
    JsonArray zoneArr = statusDoc.createNestedArray("zones");
    statusDoc["pumpAmps"] = 2.3;
    statusDoc["interval"] = interval;

    for (size_t i = 0; i < zones.size(); i++) {
      JsonObject z = zoneArr.createNestedObject();
      z["id"] = i;
      z["name"] = zones[i].name;
      z["status"] = zones[i].enabled ? "Idle" : "Disabled";
      z["lastRun"] = millis();
      z["pin"] = zones[i].pin;
      z["enabled"] = zones[i].enabled;
    }

    String out;
    serializeJson(statusDoc, out);
    request->send(200, "application/json", out);
  });

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
        zones.push_back(zone);
      }
      saveConfig();
      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.begin();
}

void loop() {}

String serializeJsonToString(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  return out;
}
