#pragma once

#include <Arduino.h>

static constexpr int kNumZones = 4;
static constexpr size_t kLogCapacity = 200;

// Scheduler defaults
static constexpr unsigned long DEFAULT_RUN_INTERVAL_HOURS = 1;
static constexpr unsigned long DEFAULT_PRIME_DELAY_MS = 3000;
static constexpr unsigned long DEFAULT_ZONE_MAX_RUN_MS = 10000;
static constexpr unsigned long DEFAULT_SAMPLE_INTERVAL_MS = 100;
static constexpr unsigned long DEFAULT_POST_ZONE_SETTLE_MS = 300;

// Current thresholds in amps.
static constexpr float DEFAULT_WET_CURRENT_A = 2.0f;
static constexpr float DEFAULT_DRY_CURRENT_A = 0.5f;
static constexpr float DEFAULT_FAULT_CURRENT_A = 15.0f;

// Network
static constexpr const char* DEFAULT_AP_SSID = "BilgeDry";
static constexpr const char* DEFAULT_AP_PASSWORD = "bilgedry123";
static constexpr const char* DEFAULT_MDNS_HOST = "bilgedry";
static constexpr unsigned long DEFAULT_WIFI_CONNECT_TIMEOUT_MS = 12000;

// Time
static constexpr const char* DEFAULT_NTP_SERVER_1 = "pool.ntp.org";
static constexpr const char* DEFAULT_NTP_SERVER_2 = "time.nist.gov";
static constexpr long DEFAULT_GMT_OFFSET_SEC = 0;
static constexpr int DEFAULT_DAYLIGHT_OFFSET_SEC = 0;

// Zone defaults
static constexpr const char* DEFAULT_ZONE_NAMES[kNumZones] = {
  "Forward Bilge",
  "Mid Bilge",
  "Aft Bilge",
  "Spare"
};

static constexpr bool DEFAULT_ZONE_ENABLED[kNumZones] = {
  true,
  true,
  true,
  false
};
