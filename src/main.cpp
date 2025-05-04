#include "config.h"
#include "nmea_messages.h"
#include <Adafruit_INA260.h>
#include <EEPROM.h>

// INA260 sensor
Adafruit_INA260 ina260;

// Timing and state
unsigned long lastRunTime = 0;
bool pumpRunning = false;

// EEPROM log struct
struct ZoneLog {
  uint32_t timestamp;
  float peakCurrent;
  uint16_t duration;
  uint8_t result; // 0=OK, 1=DRY, 2=TIMEOUT, 3=FAIL
};

ZoneLog logs[NUM_ZONES];

void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  for (int i = 0; i < NUM_ZONES; i++) {
    pinMode(SOLENOID_PINS[i], OUTPUT);
  }
  if (!ina260.begin()) {
    sendN2KStatus("INA260 init failed");
    while (1);
  }
  sendN2KStatus("System OK");
}

void loop() {
  unsigned long now = millis();
  if (now - lastRunTime >= CYCLE_INTERVAL) {
    lastRunTime = now;
    runBilgeCycle();
  }

  // Optional: Handle NMEA2000 message triggers here
}

void runBilgeCycle() {
  for (int i = 0; i < NUM_ZONES; i++) {
    float peakCurrent = 0.0;
    unsigned long start = millis();
    bool retry = false;

    digitalWrite(PUMP_PIN, HIGH);
    delay(PUMP_PRIME_DELAY);
    digitalWrite(SOLENOID_PINS[i], HIGH);

    while (millis() - start < MAX_ZONE_RUNTIME) {
      float current = ina260.readCurrent();
      if (current > peakCurrent) peakCurrent = current;

      if (current > CURRENT_THRESHOLD) {
        while (ina260.readCurrent() > CURRENT_THRESHOLD) {
          delay(500);
          if (millis() - start >= MAX_ZONE_RUNTIME) break;
        }
        break;
      }
      delay(200);
    }

    digitalWrite(SOLENOID_PINS[i], LOW);
    digitalWrite(PUMP_PIN, LOW);
    delay(1000);

    ZoneLog log = {now(), peakCurrent, (uint16_t)(millis() - start), 0};
    if (peakCurrent < CURRENT_THRESHOLD / 2.0) {
      log.result = 1;  // DRY
      retry = true;
    } else if (millis() - start >= MAX_ZONE_RUNTIME) {
      log.result = 2;  // TIMEOUT
      retry = true;
    }

    if (retry) {
      delay(2000);
      // Optional retry logic can be placed here
    }

    EEPROM.put(i * sizeof(ZoneLog), log);
    sendZoneStatusN2K(i, log);
  }
}
