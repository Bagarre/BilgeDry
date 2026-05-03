#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_INA260.h>
#include <time.h>
#include "Defaults.h"

enum class NetworkMode {
  Offline,
  Station,
  AccessPoint
};

enum class CycleState {
  Idle,
  Prime,
  Running,
  Settling,
  Complete,
  Fault
};

struct ZoneConfig {
  String name;
  bool enabled;
};

struct RuntimeConfig {
  bool systemEnabled;
  bool demoMode;
  unsigned long runIntervalMs;
  unsigned long primeDelayMs;
  unsigned long zoneMaxRunMs;
  unsigned long sampleIntervalMs;
  unsigned long settleDelayMs;
  float wetCurrentA;
  float dryCurrentA;
  float faultCurrentA;
};

struct NetworkConfig {
  String wifiSsid;
  String wifiPassword;
  String apSsid;
  String apPassword;
  String mdnsHost;
};

struct ZoneResult {
  bool attempted;
  bool active;
  bool detectedWater;
  bool timedOut;
  bool faulted;
  float startCurrentA;
  float peakCurrentA;
  float endCurrentA;
  unsigned long durationMs;
  unsigned long lastSuccessEpoch;
};

struct SchedulerState {
  unsigned long lastRunMillis;
  unsigned long nextRunMillis;
  time_t lastRunEpoch;
  time_t nextRunEpoch;
};

struct CycleRuntime {
  bool active;
  bool runRequested;
  bool abortRequested;
  int activeZone;
  int zonesCompleted;
  int wetZonesThisCycle;
  int dryZonesThisCycle;
  CycleState state;
  String stateText;
  String lastEvent;
  unsigned long cycleStartedMs;
  unsigned long stateStartedMs;
  unsigned long lastSampleMs;
  unsigned long completedCycles;
  unsigned long faultCount;
  float latestCurrentA;
  float peakCurrentA;
  String faultMessage;
};

struct NetworkStatus {
  NetworkMode mode;
  String currentSsid;
  String ipAddress;
  bool timeSynced;
};

struct AppState {
  RuntimeConfig runtime;
  NetworkConfig network;
  SchedulerState scheduler;
  CycleRuntime cycle;
  NetworkStatus networkStatus;
  ZoneConfig zones[kNumZones];
  ZoneResult zoneResults[kNumZones];
};

struct AppContext {
  AsyncWebServer server;
  Adafruit_INA260 ina260;
  AppState state;
  bool currentSensorReady;

  AppContext() : server(80), currentSensorReady(false) {}
};
