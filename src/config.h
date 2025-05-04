#pragma once

#define NUM_ZONES 4
#define PUMP_PIN 5
#define SOLENOID_PINS {6, 7, 8, 9}

#define CURRENT_THRESHOLD 300.0  // milliamps
#define MAX_ZONE_RUNTIME 180000  // 3 minutes in ms
#define PUMP_PRIME_DELAY 2000  // 2 seconds
#define CYCLE_INTERVAL 3600000  // 1 hour in ms
