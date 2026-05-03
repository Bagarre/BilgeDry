#include "NetworkManager.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include "Defaults.h"
#include "LogManager.h"

namespace {
void syncTime(AppContext& ctx) {
  configTime(
    DEFAULT_GMT_OFFSET_SEC,
    DEFAULT_DAYLIGHT_OFFSET_SEC,
    DEFAULT_NTP_SERVER_1,
    DEFAULT_NTP_SERVER_2);

  struct tm timeInfo;
  const unsigned long started = millis();
  while (!getLocalTime(&timeInfo) && (millis() - started) < 10000UL) {
    delay(250);
  }

  ctx.state.networkStatus.timeSynced = getLocalTime(&timeInfo, 10);
  if (ctx.state.networkStatus.timeSynced) {
    LogManager::info("Time synchronized");
  } else {
    LogManager::warn("Time sync failed; epoch values may be zero until network time is available");
  }
}

bool connectSta(AppContext& ctx) {
  if (ctx.state.network.wifiSsid.isEmpty()) {
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ctx.state.network.wifiSsid.c_str(), ctx.state.network.wifiPassword.c_str());

  const unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - started) < DEFAULT_WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    ctx.state.networkStatus.mode = NetworkMode::Station;
    ctx.state.networkStatus.currentSsid = WiFi.SSID();
    ctx.state.networkStatus.ipAddress = WiFi.localIP().toString();
    LogManager::info("Connected to Wi-Fi SSID: " + WiFi.SSID());
    return true;
  }

  return false;
}

void startAp(AppContext& ctx) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ctx.state.network.apSsid.c_str(), ctx.state.network.apPassword.c_str());
  ctx.state.networkStatus.mode = NetworkMode::AccessPoint;
  ctx.state.networkStatus.currentSsid = WiFi.softAPSSID();
  ctx.state.networkStatus.ipAddress = WiFi.softAPIP().toString();
  LogManager::warn("Started AP mode SSID: " + ctx.state.networkStatus.currentSsid);
}
}

namespace NetworkManager {

void begin(AppContext& ctx) {
  if (!connectSta(ctx)) {
    startAp(ctx);
  }

  if (MDNS.begin(ctx.state.network.mdnsHost.c_str())) {
    LogManager::info("mDNS active at http://" + ctx.state.network.mdnsHost + ".local");
  } else {
    LogManager::warn("mDNS start failed");
  }

  syncTime(ctx);
}

void reboot() {
  delay(250);
  ESP.restart();
}

}
