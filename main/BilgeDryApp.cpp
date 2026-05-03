#include "BilgeDryApp.h"

#include <Arduino.h>
#include "AppTypes.h"
#include "AppVersion.h"
#include "CycleController.h"
#include "LogManager.h"
#include "NetworkManager.h"
#include "SensorManager.h"
#include "Storage.h"
#include "WebServerManager.h"

namespace {
AppContext gCtx;
}

namespace BilgeDryApp {

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!Storage::begin()) {
    return;
  }

  Storage::loadDefaults(gCtx.state);
  LogManager::begin();
  LogManager::info(String(APP_NAME) + " " + APP_VERSION + " booting");

  Storage::loadAll(gCtx.state);
  SensorManager::begin(gCtx);
  CycleController::begin(gCtx);
  NetworkManager::begin(gCtx);
  CycleController::scheduleFromNow(gCtx.state);
  WebServerManager::begin(gCtx);

  LogManager::info("Boot complete");
}

void loop() {
  CycleController::tick(gCtx);
  delay(10);
}

}
