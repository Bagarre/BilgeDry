#include "SensorManager.h"

#include "LogManager.h"

namespace SensorManager {

void begin(AppContext& ctx) {
  ctx.currentSensorReady = ctx.ina260.begin();
  if (ctx.currentSensorReady) {
    LogManager::info("INA260 initialized");
  } else {
    LogManager::error("INA260 not detected; current sensing disabled");
  }
}

float readCurrentAmps(AppContext& ctx) {
  if (!ctx.currentSensorReady) {
    return 0.0f;
  }
  return ctx.ina260.readCurrent() / 1000.0f;
}

}
