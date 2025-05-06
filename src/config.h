/*
 * BilgeDry - System Configuration Header
 * --------------------------------------
 * config.h
 *
 * Description:
 *   This header file defines system-wide constants and hardware mappings
 *   for the BilgeDry dry bilge controller. It specifies the number of zones,
 *   pin assignments for the pump and solenoids, current thresholds for water
 *   detection, timing parameters for pump operation, and the interval between
 *   automatic bilge cycles.
 *
 * Functionality:
 *   - Defines the number of monitored zones
 *   - Assigns microcontroller pins for pump and solenoid control
 *   - Sets the current threshold to detect water presence
 *   - Specifies maximum runtime for each zone to prevent overuse
 *   - Sets pump priming delay to ensure proper operation
 *   - Determines the interval between automatic bilge cycles
 *
 * Author: Bagarre
 * Repository: https://github.com/Bagarre/BilgeDry
 * Date: 2025-05-05
 */

#pragma once  // Ensure this header is included only once during compilation

// Number of bilge zones to monitor
#define NUM_ZONES 4

// Microcontroller pin connected to the pump relay
#define PUMP_PIN 5

// Microcontroller pins connected to the solenoid valves for each zone
#define SOLENOID_PINS {6, 7, 8, 9}

// Current threshold (in milliamps) to detect water presence
#define CURRENT_THRESHOLD 300.0  // mA

// Maximum allowed runtime for a zone before timing out (in milliseconds)
#define MAX_ZONE_RUNTIME 180000  // 3 minutes

// Delay after activating the pump to allow it to prime (in milliseconds)
#define PUMP_PRIME_DELAY 2000  // 2 seconds

// Interval between automatic bilge cycles (in milliseconds)
#define CYCLE_INTERVAL 3600000  // 1 hour
