#include "CycleController.h"

#include "LogManager.h"
#include "Pins.h"
#include "SensorManager.h"
#include "Storage.h"

namespace {
void allOutputsOff() {
  digitalWrite(PUMP_PIN, LOW);
  for (int i = 0; i < kNumZones; ++i) {
    digitalWrite(SOLENOID_PINS[i], LOW);
  }
}

void setZoneOutput(int zone, bool on) {
  for (int i = 0; i < kNumZones; ++i) {
    digitalWrite(SOLENOID_PINS[i], i == zone && on ? HIGH : LOW);
  }
  digitalWrite(PUMP_PIN, on ? HIGH : LOW);
}

String cycleStateText(CycleState state) {
  switch (state) {
    case CycleState::Idle: return "idle";
    case CycleState::Prime: return "prime";
    case CycleState::Running: return "running";
    case CycleState::Settling: return "settling";
    case CycleState::Complete: return "complete";
    case CycleState::Fault: return "fault";
  }
  return "unknown";
}

void setState(AppContext& ctx, CycleState state, const String& event) {
  ctx.state.cycle.state = state;
  ctx.state.cycle.stateText = cycleStateText(state);
  ctx.state.cycle.stateStartedMs = millis();
  ctx.state.cycle.lastEvent = event;
}

void setFault(AppContext& ctx, const String& message) {
  allOutputsOff();
  ctx.state.cycle.active = false;
  ctx.state.cycle.faultCount++;
  ctx.state.cycle.faultMessage = message;
  setState(ctx, CycleState::Fault, message);
  LogManager::error(message);
}

void resetZoneResult(ZoneResult& result) {
  result.attempted = false;
  result.active = false;
  result.detectedWater = false;
  result.timedOut = false;
  result.faulted = false;
  result.startCurrentA = 0.0f;
  result.peakCurrentA = 0.0f;
  result.endCurrentA = 0.0f;
  result.durationMs = 0;
}

void startCycle(AppContext& ctx) {
  for (int i = 0; i < kNumZones; ++i) {
    resetZoneResult(ctx.state.zoneResults[i]);
  }

  ctx.state.cycle.active = true;
  ctx.state.cycle.runRequested = false;
  ctx.state.cycle.abortRequested = false;
  ctx.state.cycle.activeZone = -1;
  ctx.state.cycle.zonesCompleted = 0;
  ctx.state.cycle.wetZonesThisCycle = 0;
  ctx.state.cycle.dryZonesThisCycle = 0;
  ctx.state.cycle.latestCurrentA = 0.0f;
  ctx.state.cycle.peakCurrentA = 0.0f;
  ctx.state.cycle.faultMessage = "";
  ctx.state.cycle.cycleStartedMs = millis();
  ctx.state.cycle.completedCycles++;

  ctx.state.scheduler.lastRunMillis = millis();
  ctx.state.scheduler.lastRunEpoch = time(nullptr);
  setState(ctx, CycleState::Prime, "Cycle started");
  LogManager::info("Cycle started");
}

bool advanceToNextZone(AppContext& ctx) {
  for (int next = ctx.state.cycle.activeZone + 1; next < kNumZones; ++next) {
    if (ctx.state.zones[next].enabled) {
      ctx.state.cycle.activeZone = next;
      ZoneResult& result = ctx.state.zoneResults[next];
      result.attempted = true;
      result.active = true;
      result.startCurrentA = SensorManager::readCurrentAmps(ctx);
      result.peakCurrentA = result.startCurrentA;
      result.detectedWater = result.startCurrentA >= ctx.state.runtime.wetCurrentA;
      ctx.state.cycle.lastSampleMs = millis();
      ctx.state.cycle.peakCurrentA = max(ctx.state.cycle.peakCurrentA, result.peakCurrentA);
      setZoneOutput(next, true);
      setState(ctx, CycleState::Prime, "Priming " + ctx.state.zones[next].name);
      LogManager::info("Zone start: " + ctx.state.zones[next].name);
      return true;
    }
  }
  return false;
}

void finishCurrentZone(AppContext& ctx, bool timedOut, bool detectedWater) {
  const int zone = ctx.state.cycle.activeZone;
  if (zone < 0 || zone >= kNumZones) {
    return;
  }

  ZoneResult& result = ctx.state.zoneResults[zone];
  result.active = false;
  result.endCurrentA = ctx.state.cycle.latestCurrentA;
  result.durationMs = millis() - ctx.state.cycle.stateStartedMs;
  result.timedOut = timedOut;
  result.detectedWater = result.detectedWater || detectedWater;
  if (result.detectedWater) {
    result.lastSuccessEpoch = time(nullptr);
    ctx.state.cycle.wetZonesThisCycle++;
  } else {
    ctx.state.cycle.dryZonesThisCycle++;
  }

  allOutputsOff();
  ctx.state.cycle.zonesCompleted++;

  String summary = ctx.state.zones[zone].name + " finished in " + String(result.durationMs / 1000.0f, 1) +
                   "s, peak " + String(result.peakCurrentA, 2) + "A";
  if (timedOut) {
    summary += " (timeout)";
  } else if (!detectedWater) {
    summary += " (dry)";
  } else {
    summary += " (wet)";
  }
  LogManager::info(summary);

  setState(ctx, CycleState::Settling, "Settling after " + ctx.state.zones[zone].name);
  Storage::saveRuntimeConfig(ctx.state);
}

void completeCycle(AppContext& ctx) {
  allOutputsOff();
  ctx.state.cycle.active = false;
  setState(ctx, CycleState::Complete,
           "Cycle complete: " + String(ctx.state.cycle.wetZonesThisCycle) + " wet, " +
           String(ctx.state.cycle.dryZonesThisCycle) + " dry");
  CycleController::scheduleFromNow(ctx.state);
  LogManager::info(ctx.state.cycle.lastEvent);
}
}

namespace CycleController {

void begin(AppContext& ctx) {
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  for (int i = 0; i < kNumZones; ++i) {
    pinMode(SOLENOID_PINS[i], OUTPUT);
    digitalWrite(SOLENOID_PINS[i], LOW);
  }
  ctx.state.cycle.state = CycleState::Idle;
  ctx.state.cycle.stateText = "idle";
  ctx.state.cycle.activeZone = -1;
}

void scheduleFromNow(AppState& state) {
  const unsigned long nowMs = millis();
  const time_t nowEpoch = time(nullptr);
  state.scheduler.nextRunMillis = nowMs + state.runtime.runIntervalMs;
  state.scheduler.nextRunEpoch = nowEpoch ? nowEpoch + (state.runtime.runIntervalMs / 1000UL) : 0;
}

void requestRun(AppContext& ctx) {
  ctx.state.cycle.runRequested = true;
}

void requestAbort(AppContext& ctx) {
  ctx.state.cycle.abortRequested = true;
}

void tick(AppContext& ctx) {
  ctx.state.cycle.latestCurrentA = SensorManager::readCurrentAmps(ctx);

  if (ctx.state.cycle.abortRequested) {
    ctx.state.cycle.abortRequested = false;
    setFault(ctx, "Cycle aborted by request");
    scheduleFromNow(ctx.state);
    return;
  }

  if (!ctx.state.runtime.systemEnabled) {
    allOutputsOff();
    if (ctx.state.cycle.active) {
      setFault(ctx, "System disabled during active cycle");
    }
    return;
  }

  if (!ctx.state.cycle.active) {
    const bool due = millis() >= ctx.state.scheduler.nextRunMillis;
    if (ctx.state.cycle.runRequested || due) {
      startCycle(ctx);
    } else {
      setState(ctx, CycleState::Idle, "Waiting for next cycle");
      return;
    }
  }

  if (ctx.state.runtime.demoMode) {
    for (int i = 0; i < kNumZones; ++i) {
      if (ctx.state.zones[i].enabled) {
        ctx.state.zoneResults[i].attempted = true;
        ctx.state.zoneResults[i].detectedWater = random(0, 100) > 35;
        ctx.state.zoneResults[i].durationMs = random(400, ctx.state.runtime.zoneMaxRunMs);
        ctx.state.zoneResults[i].peakCurrentA = ctx.state.zoneResults[i].detectedWater ? ctx.state.runtime.wetCurrentA : ctx.state.runtime.dryCurrentA;
      }
    }
    completeCycle(ctx);
    return;
  }

  if (ctx.state.cycle.activeZone < 0 && ctx.state.cycle.state != CycleState::Settling) {
    if (!advanceToNextZone(ctx)) {
      completeCycle(ctx);
      return;
    }
  }

  const int zone = ctx.state.cycle.activeZone;
  ZoneResult& result = ctx.state.zoneResults[zone];
  result.peakCurrentA = max(result.peakCurrentA, ctx.state.cycle.latestCurrentA);
  ctx.state.cycle.peakCurrentA = max(ctx.state.cycle.peakCurrentA, ctx.state.cycle.latestCurrentA);

  switch (ctx.state.cycle.state) {
    case CycleState::Prime:
      if ((millis() - ctx.state.cycle.stateStartedMs) >= ctx.state.runtime.primeDelayMs) {
        setState(ctx, CycleState::Running, "Running " + ctx.state.zones[zone].name);
      }
      break;

    case CycleState::Running: {
      if ((millis() - ctx.state.cycle.lastSampleMs) >= ctx.state.runtime.sampleIntervalMs) {
        ctx.state.cycle.lastSampleMs = millis();

        if (ctx.state.cycle.latestCurrentA >= ctx.state.runtime.wetCurrentA) {
          result.detectedWater = true;
        }

        if (ctx.state.cycle.latestCurrentA >= ctx.state.runtime.faultCurrentA) {
          result.faulted = true;
          setFault(ctx, "Overcurrent on " + ctx.state.zones[zone].name + ": " + String(ctx.state.cycle.latestCurrentA, 2) + "A");
          scheduleFromNow(ctx.state);
          return;
        }

        const unsigned long runningMs = millis() - ctx.state.cycle.stateStartedMs;
        if (ctx.state.cycle.latestCurrentA < ctx.state.runtime.dryCurrentA) {
          finishCurrentZone(ctx, false, false);
          return;
        }

        if (runningMs >= ctx.state.runtime.zoneMaxRunMs) {
          finishCurrentZone(ctx, true, true);
          return;
        }
      }
      break;
    }

    case CycleState::Settling:
      if ((millis() - ctx.state.cycle.stateStartedMs) >= ctx.state.runtime.settleDelayMs) {
        ctx.state.cycle.activeZone = zone;
        if (!advanceToNextZone(ctx)) {
          completeCycle(ctx);
        }
      }
      break;

    case CycleState::Complete:
    case CycleState::Fault:
    case CycleState::Idle:
      break;
  }
}

}
