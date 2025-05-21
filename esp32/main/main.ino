// File: main.ino
// Title: BilgeDry 4-Zone Dry Pump Controller with Demo Mode
// -----------------------------------------------------------

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Adafruit_INA260.h>
#include "config.h"
#include <time.h>

// === Constants & Globals ===
const int kNumZones = 4;
const int solenoidPins[kNumZones] = {14, 27, 26, 25};
const int pumpPin = 32;
bool systemEnabled = true;

struct Zone {
  String name;
  bool   enabled;
  unsigned long lastDuration;  // seconds
};

Zone zones[kNumZones];

unsigned long runFrequencyMs;
unsigned long runDelayMs;
float        runCurrentThreshold;
unsigned long runMaxMs;

unsigned long lastRunTime = 0;
unsigned long nextRunTime = 0;

bool demoMode = true;           // Toggle demo data

AsyncWebServer server(80);
Adafruit_INA260 ina260;

// === Function Declarations ===
void loadConfig();
void saveConfig();
void performCycle();
void demoCycle();
float readCurrent();
void logEvent(const String &event);

void handleGetStatus(AsyncWebServerRequest* req);
void handleManualRun(AsyncWebServerRequest* req);
void handleGetConfig(AsyncWebServerRequest* req);
void handleGetNetwork(AsyncWebServerRequest* req);
void handleGetLogs(AsyncWebServerRequest* req);

// === Setup ===
void setup() {
  Serial.begin(115200);


  // Configure timezone and NTP servers
    const char* ntpServer = "pool.ntp.org";
    const long  gmtOffset_sec = 0;      // adjust to your zone
    const int   daylightOffset_sec = 0;
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // (optional) wait for time to be set
    Serial.print("Waiting for NTP time");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {  // crude check
      delay(500);
      Serial.print(".");
      now = time(nullptr);
    }
    Serial.println();
    Serial.print("Current epoch: "); Serial.println(now);
    // … rest of setup …


  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, LOW);  // ensure pump is off at boot

  Serial.println("Booting...");

  if (!ina260.begin()) {
    Serial.println("Couldn't find INA260 sensor!");
  } else {
    Serial.println("INA260 initialized.");
  }

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  randomSeed(millis());

  // Initialize solenoid pins
  for (int i = 0; i < kNumZones; ++i) {
    pinMode(solenoidPins[i], OUTPUT);
    digitalWrite(solenoidPins[i], LOW);
  }

  // Load configuration and schedule first run
  loadConfig();
  lastRunTime = millis();
  nextRunTime = lastRunTime + runFrequencyMs;

  // Connect to Wi-Fi (replace SSID/PASSWORD)
  setupWiFi();

  if (MDNS.begin("BilgeDry")) {
    Serial.println("mDNS started: http://bilgedry.local");
  }

  // REST API endpoints
  server.on("/api/v1/status",     HTTP_GET,  handleGetStatus);


  server.on("/api/v1/reboot", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "application/text", "Robooting");
    ESP.restart();
  });



  server.on("/api/v1/enable", HTTP_GET, [](AsyncWebServerRequest *request){
      StaticJsonDocument<64> doc;
      doc["enabled"] = systemEnabled;
      String out;
      serializeJson(doc, out);
      request->send(200, "application/json", out);
    });

  server.on("/api/v1/enable", HTTP_POST, [](AsyncWebServerRequest *request){}, nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<64> doc;
      if (deserializeJson(doc, data)) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }
      systemEnabled = doc["enabled"];
//      if (!systemEnabled) disableField("User disabled");
      request->send(200, "application/json", "{\"status\":\"updated\"}");
  });




  server.on("/api/v1/status/run", HTTP_POST, handleManualRun);
  server.on("/api/v1/config",     HTTP_GET,  handleGetConfig);
  server.on("/api/v1/config",     HTTP_POST,
                                              handleConfigRequest,   // onRequest
                                              nullptr,               // onUpload (not used)
                                              handleConfigBody       // onBody
                                            );


  server.on("/api/v1/network",    HTTP_GET,  handleGetNetwork);
  server.on("/api/v1/logs",       HTTP_GET,  handleGetLogs);
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.serveStatic("/style.css", SPIFFS, "/style.css");
  server.serveStatic("/app.js", SPIFFS, "/app.js");
  server.begin();

}

// === Main Loop ===
void loop() {
  delay(500);
  if (demoMode) {
    demoCycle();
    delay(runFrequencyMs);
    return;
  }

  unsigned long now = millis();
  if (now >= nextRunTime) {
    lastRunTime = now;
    performCycle();
    nextRunTime = now + runFrequencyMs;
    logEvent("Cycle completed");
  }
}

// === Demo Mode ===
void demoCycle() {
  unsigned long now = millis();
  lastRunTime = now;
  nextRunTime = now + runFrequencyMs;

  for (int i = 0; i < kNumZones; ++i) {
    if (zones[i].enabled) {
      zones[i].lastDuration = 1 + (rand() % (runMaxMs / 1000UL));
    } else {
      zones[i].lastDuration = 0;
    }
    logEvent("Demo: Zone " + String(i+1) + " ran " + String(zones[i].lastDuration) + "s");
  }
}


void setupWiFi() {
  // if (ssid[0] == '\0' || password[0] == '\0') {
  //   Serial.println("Starting Access Point (no SSID provided)...");
  //   WiFi.softAP(fallbackAP, fallbackPass);
  //   Serial.print("AP IP address: ");
  //   Serial.println(WiFi.softAPIP());
  //   return;
  // }

  WiFi.begin("Bagarre", "Nitt4agm2!");
  Serial.print("Connecting to WiFi");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi failed — starting fallback AP...");
//    WiFi.softAP(fallbackAP, fallbackPass);
//    Serial.print("AP IP address: ");
//    Serial.println(WiFi.softAPIP());
  }
}

// === Configuration Persistence ===
void loadConfig() {
  File f = SPIFFS.open("/config.json", "r");
  if (!f) {
    // No config file → use compile-time defaults
    runFrequencyMs      = DEFAULT_RUN_FREQUENCY_H * 3600000UL;
    runDelayMs          = DEFAULT_RUN_DELAY_S     * 1000UL;
    runCurrentThreshold = DEFAULT_RUN_CURRENT_A;
    runMaxMs            = DEFAULT_RUN_MAX_S       * 1000UL;
    zones[0] = { DEFAULT_ZONE1_NAME, DEFAULT_ZONE1_ENABLED, 0 };
    zones[1] = { DEFAULT_ZONE2_NAME, DEFAULT_ZONE2_ENABLED, 0 };
    zones[2] = { DEFAULT_ZONE3_NAME, DEFAULT_ZONE3_ENABLED, 0 };
    zones[3] = { DEFAULT_ZONE4_NAME, DEFAULT_ZONE4_ENABLED, 0 };
    return;
  }

  StaticJsonDocument<512> doc;
  auto err = deserializeJson(doc, f);
  f.close();
  if (err) {
    // Parse error → fallback to defaults
    runFrequencyMs      = DEFAULT_RUN_FREQUENCY_H * 3600000UL;
    runDelayMs          = DEFAULT_RUN_DELAY_S     * 1000UL;
    runCurrentThreshold = DEFAULT_RUN_CURRENT_A;
    runMaxMs            = DEFAULT_RUN_MAX_S       * 1000UL;
    zones[0] = { DEFAULT_ZONE1_NAME, DEFAULT_ZONE1_ENABLED, 0 };
    zones[1] = { DEFAULT_ZONE2_NAME, DEFAULT_ZONE2_ENABLED, 0 };
    zones[2] = { DEFAULT_ZONE3_NAME, DEFAULT_ZONE3_ENABLED, 0 };
    zones[3] = { DEFAULT_ZONE4_NAME, DEFAULT_ZONE4_ENABLED, 0 };
    return;
  }

  // Load user‐set values
  runFrequencyMs      = doc["runFrequencyH"].as<unsigned long>() * 3600000UL;
  runDelayMs          = doc["runDelayS"].as<unsigned long>()     * 1000UL;
  runCurrentThreshold = doc["runCurrentA"].as<float>();
  runMaxMs            = doc["runMaxS"].as<unsigned long>()       * 1000UL;

  for (int i = 0; i < kNumZones; ++i) {
    zones[i].name    = doc["zones"][i]["name"].as<String>();
    zones[i].enabled = doc["zones"][i]["enabled"].as<bool>();
  }
}


void saveConfig() {
  StaticJsonDocument<512> doc;
  doc["runFrequencyH"] = runFrequencyMs / 3600000UL;
  doc["runDelayS"]     = runDelayMs / 1000UL;
  doc["runCurrentA"]   = runCurrentThreshold;
  doc["runMaxS"]       = runMaxMs / 1000UL;
  JsonArray arr = doc.createNestedArray("zones");
  for (int i = 0; i < kNumZones; ++i) {
    JsonObject z = arr.createNestedObject();
    z["name"]    = zones[i].name;
    z["enabled"] = zones[i].enabled;
  }
  File f = SPIFFS.open("/config.json","w");
  serializeJson(doc,f);
  f.close();
}

// === Pump Cycle ===
void performCycle() {
  for (int i = 0; i < kNumZones; ++i) {
    if (!zones[i].enabled) continue;
    digitalWrite(solenoidPins[i], HIGH);
    digitalWrite(pumpPin, HIGH);
    delay(runDelayMs);
    unsigned long start = millis();
    if (readCurrent() >= runCurrentThreshold) {
      while (millis() - start < runMaxMs) {
        if (readCurrent() < runCurrentThreshold) break;
        delay(100);
      }
    }
    zones[i].lastDuration = (millis() - start) / 1000UL;
    digitalWrite(pumpPin, LOW);
    digitalWrite(solenoidPins[i], LOW);
    logEvent("Zone " + String(i+1) + " ran " + String(zones[i].lastDuration) + "s");
  }
}

// === Sensor & Logging ===
float readCurrent() { return ina260.readCurrent(); }

void logEvent(const String &event) {
  time_t epoch = time(nullptr);
  File f = SPIFFS.open("/logs.txt", "a");
  if (!f) return;
  // epoch seconds, colon, then your event
  f.println(String(epoch) + ": " + event);
  f.close();
}
// === API Handlers ===
void handleGetStatus(AsyncWebServerRequest *req) {
  StaticJsonDocument<256> doc;
  doc["lastRun"]    = lastRunTime / 1000UL;
  doc["nextRun"]    = nextRunTime / 1000UL;
  for (int i = 0; i < kNumZones; ++i) {
    String idx = String(i+1);
    doc["port" + idx + "Name"]   = zones[i].name;
    doc["port" + idx + "Enable"] = zones[i].enabled;
    doc["port" + idx + "Run"]    = zones[i].lastDuration;
  }
  String out; serializeJson(doc,out);
  req->send(200,"application/json",out);
}


void handleManualRun(AsyncWebServerRequest *req) { performCycle(); req->send(200); }


// ----------------------------------
// 1) The onRequest stub – does nothing
// ----------------------------------
void handleConfigRequest(AsyncWebServerRequest *req) {
  // we’ll send the real response in handleConfigBody()
}

// ----------------------------------
// 2) The onBody callback – parse JSON & reply
// ----------------------------------
void handleConfigBody(AsyncWebServerRequest *req,
                      uint8_t *data, size_t len,
                      size_t index, size_t total) {
  // only parse once, when index==0
  if (index != 0) return;

  // use a heap‐based buffer
  DynamicJsonDocument doc(512);
  auto err = deserializeJson(doc, data, len);
  if (err) {
    req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  // now pull out your flat keys
  runFrequencyMs      = doc["runFrequency"].as<unsigned long>() * 3600000UL;
  runDelayMs          = doc["runDelay"].as<unsigned long>()     * 1000UL;
  runCurrentThreshold = doc["runCurrent"].as<float>();
  runMaxMs            = doc["runMax"].as<unsigned long>()       * 1000UL;

  for (int i = 0; i < kNumZones; ++i) {
    String idx = String(i+1);
    zones[i].name    = doc["port" + idx + "Name"].as<const char*>();
    zones[i].enabled = doc["port" + idx + "Enable"].as<bool>();
  }

  saveConfig();
  req->send(200, "application/json", "{\"status\":\"OK\"}");
}




void handleGetConfig(AsyncWebServerRequest *req) {
  StaticJsonDocument<256> doc;
  doc["runFrequency"] = runFrequencyMs / 3600000UL;
  doc["runDelay"]     = runDelayMs / 1000UL;
  doc["runCurrent"]   = runCurrentThreshold;
  doc["runMax"]       = runMaxMs / 1000UL;
  for (int i = 0; i < kNumZones; ++i) {
    String idx = String(i+1);
    doc["port" + idx + "Name"]   = zones[i].name;
    doc["port" + idx + "Enable"] = zones[i].enabled;
  }
  String out; serializeJson(doc,out);
  req->send(200,"application/json",out);
}

void handleGetNetwork(AsyncWebServerRequest *req) {
  StaticJsonDocument<128> doc;
  doc["ip"]   = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  String out; serializeJson(doc,out);
  req->send(200,"application/json",out);
}

void handleGetLogs(AsyncWebServerRequest *req) {
  File f = SPIFFS.open("/logs.txt","r");
  StaticJsonDocument<1024> doc;
  JsonArray arr = doc.to<JsonArray>();
  while (f.available()) arr.add(f.readStringUntil('\n'));
  f.close();
  String out; serializeJson(arr,out);
  req->send(200,"application/json",out);
}
