#include "WebServerManager.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "AppVersion.h"
#include "CycleController.h"
#include "LogManager.h"
#include "NetworkManager.h"
#include "SensorManager.h"
#include "Storage.h"

namespace {
void sendJson(AsyncWebServerRequest* request, const String& body, int status = 200) {
  request->send(status, "application/json", body);
}

bool parseJsonBody(uint8_t* data, size_t len, DynamicJsonDocument& doc) {
  return !deserializeJson(doc, data, len);
}

const char* networkModeText(NetworkMode mode) {
  switch (mode) {
    case NetworkMode::Offline: return "offline";
    case NetworkMode::Station: return "station";
    case NetworkMode::AccessPoint: return "ap";
  }
  return "unknown";
}

String buildStatusJson(AppContext& ctx) {
  DynamicJsonDocument doc(4096);
  doc["appName"] = APP_NAME;
  doc["appVersion"] = APP_VERSION;
  doc["buildDate"] = APP_BUILD_DATE;
  doc["enabled"] = ctx.state.runtime.systemEnabled;
  doc["demoMode"] = ctx.state.runtime.demoMode;
  doc["currentA"] = SensorManager::readCurrentAmps(ctx);
  doc["cycleState"] = ctx.state.cycle.stateText;
  doc["cycleActive"] = ctx.state.cycle.active;
  doc["cycleEvent"] = ctx.state.cycle.lastEvent;
  doc["faultMessage"] = ctx.state.cycle.faultMessage;
  doc["activeZone"] = ctx.state.cycle.activeZone;
  doc["peakCurrentA"] = ctx.state.cycle.peakCurrentA;
  doc["completedCycles"] = ctx.state.cycle.completedCycles;
  doc["faultCount"] = ctx.state.cycle.faultCount;
  doc["lastRun"] = static_cast<unsigned long>(ctx.state.scheduler.lastRunEpoch);
  doc["nextRun"] = static_cast<unsigned long>(ctx.state.scheduler.nextRunEpoch);

  JsonArray zones = doc.createNestedArray("zones");
  for (int i = 0; i < kNumZones; ++i) {
    JsonObject z = zones.createNestedObject();
    z["index"] = i;
    z["name"] = ctx.state.zones[i].name;
    z["enabled"] = ctx.state.zones[i].enabled;
    z["attempted"] = ctx.state.zoneResults[i].attempted;
    z["active"] = ctx.state.zoneResults[i].active;
    z["detectedWater"] = ctx.state.zoneResults[i].detectedWater;
    z["timedOut"] = ctx.state.zoneResults[i].timedOut;
    z["faulted"] = ctx.state.zoneResults[i].faulted;
    z["durationMs"] = ctx.state.zoneResults[i].durationMs;
    z["startCurrentA"] = ctx.state.zoneResults[i].startCurrentA;
    z["peakCurrentA"] = ctx.state.zoneResults[i].peakCurrentA;
    z["endCurrentA"] = ctx.state.zoneResults[i].endCurrentA;
    z["lastSuccessEpoch"] = ctx.state.zoneResults[i].lastSuccessEpoch;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

String buildConfigJson(const AppContext& ctx) {
  DynamicJsonDocument doc(3072);
  doc["systemEnabled"] = ctx.state.runtime.systemEnabled;
  doc["demoMode"] = ctx.state.runtime.demoMode;
  doc["runIntervalHours"] = ctx.state.runtime.runIntervalMs / 3600000UL;
  doc["primeDelayMs"] = ctx.state.runtime.primeDelayMs;
  doc["zoneMaxRunMs"] = ctx.state.runtime.zoneMaxRunMs;
  doc["sampleIntervalMs"] = ctx.state.runtime.sampleIntervalMs;
  doc["settleDelayMs"] = ctx.state.runtime.settleDelayMs;
  doc["wetCurrentA"] = ctx.state.runtime.wetCurrentA;
  doc["dryCurrentA"] = ctx.state.runtime.dryCurrentA;
  doc["faultCurrentA"] = ctx.state.runtime.faultCurrentA;

  JsonArray zones = doc.createNestedArray("zones");
  for (int i = 0; i < kNumZones; ++i) {
    JsonObject z = zones.createNestedObject();
    z["index"] = i;
    z["name"] = ctx.state.zones[i].name;
    z["enabled"] = ctx.state.zones[i].enabled;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

String buildNetworkJson(const AppContext& ctx) {
  DynamicJsonDocument doc(1024);
  doc["mode"] = networkModeText(ctx.state.networkStatus.mode);
  doc["ip"] = ctx.state.networkStatus.ipAddress;
  doc["currentSSID"] = ctx.state.networkStatus.currentSsid;
  doc["timeSynced"] = ctx.state.networkStatus.timeSynced;
  doc["wifiSSID"] = ctx.state.network.wifiSsid;
  doc["apSSID"] = ctx.state.network.apSsid;
  doc["mdnsHost"] = ctx.state.network.mdnsHost;
  String out;
  serializeJson(doc, out);
  return out;
}

void handleConfigPost(AppContext& ctx, AsyncWebServerRequest* request, uint8_t* data, size_t len) {
  DynamicJsonDocument doc(3072);
  if (!parseJsonBody(data, len, doc)) {
    sendJson(request, "{\"error\":\"Invalid JSON\"}", 400);
    return;
  }

  ctx.state.runtime.systemEnabled = doc["systemEnabled"] | ctx.state.runtime.systemEnabled;
  ctx.state.runtime.demoMode = doc["demoMode"] | ctx.state.runtime.demoMode;
  ctx.state.runtime.runIntervalMs = (doc["runIntervalHours"] | (ctx.state.runtime.runIntervalMs / 3600000UL)) * 3600000UL;
  ctx.state.runtime.primeDelayMs = doc["primeDelayMs"] | ctx.state.runtime.primeDelayMs;
  ctx.state.runtime.zoneMaxRunMs = doc["zoneMaxRunMs"] | ctx.state.runtime.zoneMaxRunMs;
  ctx.state.runtime.sampleIntervalMs = doc["sampleIntervalMs"] | ctx.state.runtime.sampleIntervalMs;
  ctx.state.runtime.settleDelayMs = doc["settleDelayMs"] | ctx.state.runtime.settleDelayMs;
  ctx.state.runtime.wetCurrentA = doc["wetCurrentA"] | ctx.state.runtime.wetCurrentA;
  ctx.state.runtime.dryCurrentA = doc["dryCurrentA"] | ctx.state.runtime.dryCurrentA;
  ctx.state.runtime.faultCurrentA = doc["faultCurrentA"] | ctx.state.runtime.faultCurrentA;

  JsonArray zones = doc["zones"].as<JsonArray>();
  if (!zones.isNull()) {
    for (JsonObject z : zones) {
      const int index = z["index"] | -1;
      if (index >= 0 && index < kNumZones) {
        ctx.state.zones[index].name = z["name"] | ctx.state.zones[index].name;
        ctx.state.zones[index].enabled = z["enabled"] | ctx.state.zones[index].enabled;
      }
    }
  }

  Storage::saveRuntimeConfig(ctx.state);
  CycleController::scheduleFromNow(ctx.state);
  LogManager::info("Runtime configuration saved");
  sendJson(request, "{\"status\":\"saved\"}");
}

void handleNetworkPost(AppContext& ctx, AsyncWebServerRequest* request, uint8_t* data, size_t len) {
  DynamicJsonDocument doc(1024);
  if (!parseJsonBody(data, len, doc)) {
    sendJson(request, "{\"error\":\"Invalid JSON\"}", 400);
    return;
  }

  ctx.state.network.wifiSsid = doc["wifiSSID"] | ctx.state.network.wifiSsid;
  ctx.state.network.wifiPassword = doc["wifiPassword"] | ctx.state.network.wifiPassword;
  ctx.state.network.apSsid = doc["apSSID"] | ctx.state.network.apSsid;
  ctx.state.network.apPassword = doc["apPassword"] | ctx.state.network.apPassword;
  ctx.state.network.mdnsHost = doc["mdnsHost"] | ctx.state.network.mdnsHost;

  Storage::saveNetworkConfig(ctx.state);
  LogManager::info("Network configuration saved");
  sendJson(request, "{\"status\":\"saved\",\"rebootRequired\":true}");
}
}

namespace WebServerManager {

void begin(AppContext& ctx) {
  ctx.server.on("/api/v1/status", HTTP_GET, [&ctx](AsyncWebServerRequest* request) {
    sendJson(request, buildStatusJson(ctx));
  });

  ctx.server.on("/api/v1/config", HTTP_GET, [&ctx](AsyncWebServerRequest* request) {
    sendJson(request, buildConfigJson(ctx));
  });

  ctx.server.on("/api/v1/config", HTTP_POST,
    [](AsyncWebServerRequest*) {}, nullptr,
    [&ctx](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      (void)index; (void)total;
      handleConfigPost(ctx, request, data, len);
    });

  ctx.server.on("/api/v1/network", HTTP_GET, [&ctx](AsyncWebServerRequest* request) {
    sendJson(request, buildNetworkJson(ctx));
  });

  ctx.server.on("/api/v1/network", HTTP_POST,
    [](AsyncWebServerRequest*) {}, nullptr,
    [&ctx](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      (void)index; (void)total;
      handleNetworkPost(ctx, request, data, len);
    });

  ctx.server.on("/api/v1/control/run", HTTP_POST, [&ctx](AsyncWebServerRequest* request) {
    CycleController::requestRun(ctx);
    sendJson(request, "{\"status\":\"queued\"}");
  });

  ctx.server.on("/api/v1/control/abort", HTTP_POST, [&ctx](AsyncWebServerRequest* request) {
    CycleController::requestAbort(ctx);
    sendJson(request, "{\"status\":\"abort_requested\"}");
  });

  ctx.server.on("/api/v1/control/enable", HTTP_POST,
    [](AsyncWebServerRequest*) {}, nullptr,
    [&ctx](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      (void)index; (void)total;
      DynamicJsonDocument doc(128);
      if (!parseJsonBody(data, len, doc)) {
        sendJson(request, "{\"error\":\"Invalid JSON\"}", 400);
        return;
      }
      ctx.state.runtime.systemEnabled = doc["enabled"] | ctx.state.runtime.systemEnabled;
      Storage::saveRuntimeConfig(ctx.state);
      LogManager::info(String("System ") + (ctx.state.runtime.systemEnabled ? "enabled" : "disabled"));
      sendJson(request, "{\"status\":\"saved\"}");
    });

  ctx.server.on("/api/v1/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendJson(request, LogManager::readAsJsonArray());
  });

  ctx.server.on("/api/v1/logs/clear", HTTP_POST, [](AsyncWebServerRequest* request) {
    LogManager::clear();
    LogManager::info("Logs cleared");
    sendJson(request, "{\"status\":\"cleared\"}");
  });

  ctx.server.on("/api/v1/reboot", HTTP_POST, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", "{\"status\":\"rebooting\"}");
    NetworkManager::reboot();
  });

  ctx.server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  ctx.server.serveStatic("/assets/", SPIFFS, "/assets/");
  ctx.server.begin();
  LogManager::info("Web server started");
}

}
