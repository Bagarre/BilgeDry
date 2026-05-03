#include "Storage.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "AppVersion.h"
#include "Defaults.h"

namespace {
auto constexpr kRuntimeConfigPath = "/runtime.json";
auto constexpr kNetworkConfigPath = "/network.json";

void setZoneDefaults(AppState& state) {
  for (int i = 0; i < kNumZones; ++i) {
    state.zones[i].name = DEFAULT_ZONE_NAMES[i];
    state.zones[i].enabled = DEFAULT_ZONE_ENABLED[i];
    state.zoneResults[i] = {};
  }
}

bool loadRuntimeFile(AppState& state) {
  File f = SPIFFS.open(kRuntimeConfigPath, FILE_READ);
  if (!f) {
    return false;
  }

  StaticJsonDocument<1536> doc;
  const auto err = deserializeJson(doc, f);
  f.close();
  if (err) {
    return false;
  }

  state.runtime.systemEnabled = doc["systemEnabled"] | true;
  state.runtime.demoMode = doc["demoMode"] | false;
  state.runtime.runIntervalMs = (doc["runIntervalHours"] | DEFAULT_RUN_INTERVAL_HOURS) * 3600000UL;
  state.runtime.primeDelayMs = doc["primeDelayMs"] | DEFAULT_PRIME_DELAY_MS;
  state.runtime.zoneMaxRunMs = doc["zoneMaxRunMs"] | DEFAULT_ZONE_MAX_RUN_MS;
  state.runtime.sampleIntervalMs = doc["sampleIntervalMs"] | DEFAULT_SAMPLE_INTERVAL_MS;
  state.runtime.settleDelayMs = doc["settleDelayMs"] | DEFAULT_POST_ZONE_SETTLE_MS;
  state.runtime.wetCurrentA = doc["wetCurrentA"] | DEFAULT_WET_CURRENT_A;
  state.runtime.dryCurrentA = doc["dryCurrentA"] | DEFAULT_DRY_CURRENT_A;
  state.runtime.faultCurrentA = doc["faultCurrentA"] | DEFAULT_FAULT_CURRENT_A;

  JsonArray zones = doc["zones"].as<JsonArray>();
  if (!zones.isNull()) {
    for (int i = 0; i < kNumZones && i < static_cast<int>(zones.size()); ++i) {
      JsonObject z = zones[i];
      state.zones[i].name = z["name"] | state.zones[i].name;
      state.zones[i].enabled = z["enabled"] | state.zones[i].enabled;
      state.zoneResults[i].lastSuccessEpoch = z["lastSuccessEpoch"] | 0UL;
    }
  }

  return true;
}

bool loadNetworkFile(AppState& state) {
  File f = SPIFFS.open(kNetworkConfigPath, FILE_READ);
  if (!f) {
    return false;
  }

  StaticJsonDocument<768> doc;
  const auto err = deserializeJson(doc, f);
  f.close();
  if (err) {
    return false;
  }

  state.network.wifiSsid = doc["wifiSSID"] | "";
  state.network.wifiPassword = doc["wifiPassword"] | "";
  state.network.apSsid = doc["apSSID"] | DEFAULT_AP_SSID;
  state.network.apPassword = doc["apPassword"] | DEFAULT_AP_PASSWORD;
  state.network.mdnsHost = doc["mdnsHost"] | DEFAULT_MDNS_HOST;
  return true;
}
}

namespace Storage {

bool begin() {
  return SPIFFS.begin(true);
}

void loadDefaults(AppState& state) {
  state.runtime.systemEnabled = true;
  state.runtime.demoMode = false;
  state.runtime.runIntervalMs = DEFAULT_RUN_INTERVAL_HOURS * 3600000UL;
  state.runtime.primeDelayMs = DEFAULT_PRIME_DELAY_MS;
  state.runtime.zoneMaxRunMs = DEFAULT_ZONE_MAX_RUN_MS;
  state.runtime.sampleIntervalMs = DEFAULT_SAMPLE_INTERVAL_MS;
  state.runtime.settleDelayMs = DEFAULT_POST_ZONE_SETTLE_MS;
  state.runtime.wetCurrentA = DEFAULT_WET_CURRENT_A;
  state.runtime.dryCurrentA = DEFAULT_DRY_CURRENT_A;
  state.runtime.faultCurrentA = DEFAULT_FAULT_CURRENT_A;

  state.network.wifiSsid = "";
  state.network.wifiPassword = "";
  state.network.apSsid = DEFAULT_AP_SSID;
  state.network.apPassword = DEFAULT_AP_PASSWORD;
  state.network.mdnsHost = DEFAULT_MDNS_HOST;

  state.scheduler = {};
  state.cycle = {};
  state.cycle.state = CycleState::Idle;
  state.cycle.stateText = "idle";
  state.cycle.activeZone = -1;
  state.networkStatus.mode = NetworkMode::Offline;
  state.networkStatus.timeSynced = false;

  setZoneDefaults(state);
}

bool loadAll(AppState& state) {
  const bool runtimeLoaded = loadRuntimeFile(state);
  const bool networkLoaded = loadNetworkFile(state);
  return runtimeLoaded || networkLoaded;
}

bool saveRuntimeConfig(const AppState& state) {
  StaticJsonDocument<1536> doc;
  doc["appName"] = APP_NAME;
  doc["appVersion"] = APP_VERSION;
  doc["systemEnabled"] = state.runtime.systemEnabled;
  doc["demoMode"] = state.runtime.demoMode;
  doc["runIntervalHours"] = state.runtime.runIntervalMs / 3600000UL;
  doc["primeDelayMs"] = state.runtime.primeDelayMs;
  doc["zoneMaxRunMs"] = state.runtime.zoneMaxRunMs;
  doc["sampleIntervalMs"] = state.runtime.sampleIntervalMs;
  doc["settleDelayMs"] = state.runtime.settleDelayMs;
  doc["wetCurrentA"] = state.runtime.wetCurrentA;
  doc["dryCurrentA"] = state.runtime.dryCurrentA;
  doc["faultCurrentA"] = state.runtime.faultCurrentA;

  JsonArray zones = doc.createNestedArray("zones");
  for (int i = 0; i < kNumZones; ++i) {
    JsonObject z = zones.createNestedObject();
    z["name"] = state.zones[i].name;
    z["enabled"] = state.zones[i].enabled;
    z["lastSuccessEpoch"] = state.zoneResults[i].lastSuccessEpoch;
  }

  File f = SPIFFS.open(kRuntimeConfigPath, FILE_WRITE);
  if (!f) {
    return false;
  }
  serializeJsonPretty(doc, f);
  f.close();
  return true;
}

bool saveNetworkConfig(const AppState& state) {
  StaticJsonDocument<768> doc;
  doc["wifiSSID"] = state.network.wifiSsid;
  doc["wifiPassword"] = state.network.wifiPassword;
  doc["apSSID"] = state.network.apSsid;
  doc["apPassword"] = state.network.apPassword;
  doc["mdnsHost"] = state.network.mdnsHost;

  File f = SPIFFS.open(kNetworkConfigPath, FILE_WRITE);
  if (!f) {
    return false;
  }
  serializeJsonPretty(doc, f);
  f.close();
  return true;
}

bool saveAll(const AppState& state) {
  return saveRuntimeConfig(state) && saveNetworkConfig(state);
}

}
