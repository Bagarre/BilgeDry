/*
 * BilgeDry - Dry Bilge Pump Controller
 * -------------------------------------
 * main.cpp
 *
 * Description:
 *   This file controls the dry bilge system using a self-priming pump,
 *   zone-based solenoids, and an Adafruit INA260 current sensor to detect water presence.
 *   It performs periodic checks of each zone, logs the results in EEPROM,
 *   and transmits zone status over NMEA 2000.
 *
 * Functionality:
 *   - Periodic bilge cycles with pump prime delay
 *   - Current-based water detection
 *   - Timeout and dry run detection with retry logic
 *   - EEPROM logging for each zone
 *   - NMEA2000 status messaging for system and zones
 *
 * Dependencies:
 *   - Adafruit_INA260 (for current sensing)
 *   - EEPROM (for persistent logging)
 *   - NMEA2000 libraries (for message transmission)
 *
 * Author: Bagarre
 * Repository: https://github.com/Bagarre/BilgeDry
 * Date: 2025-05-05
 */



// Include project-specific configuration and NMEA2000 message definitions
#include "config.h"
#include "nmea_messages.h"

// Include necessary libraries
#include <EEPROM.h>             // For storing logs in non-volatile memory
#include <Adafruit_INA260.h>    // For interfacing with the INA260 current sensor

// Instantiate the INA260 sensor object
Adafruit_INA260 ina260;

// Timing and state variables
unsigned long lastRunTime = 0;  // Tracks the last time the bilge cycle was run
bool pumpRunning = false;       // Indicates if the pump is currently running

// Structure to log data for each zone
struct ZoneLog {
    uint32_t timestamp;   // Time when the log was recorded
    float peakCurrent;    // Highest current observed during the cycle
    uint16_t duration;    // Duration of the cycle in milliseconds
    uint8_t result;       // Result code: 0=OK, 1=DRY, 2=TIMEOUT, 3=FAIL
};

// Array to store logs for each zone
ZoneLog logs[NUM_ZONES];

void setup() {
    Serial.begin(115200);  // Initialize serial communication for debugging

    // Set the pump control pin as output
    pinMode(PUMP_PIN, OUTPUT);

    // Set each solenoid control pin as output
    for (int i = 0; i < NUM_ZONES; i++) {
        pinMode(SOLENOID_PINS[i], OUTPUT);
    }

    // Initialize the INA260 sensor
    if (!ina260.begin()) {
        sendN2KStatus("INA260 init failed");  // Send error status over NMEA2000
        while (1);  // Halt execution if sensor initialization fails
    }

    sendN2KStatus("System OK");  // Send system OK status over NMEA2000
}

void loop() {
    unsigned long now = millis();  // Get the current time in milliseconds

    // Check if it's time to run the bilge cycle
    if (now - lastRunTime >= CYCLE_INTERVAL) {
        lastRunTime = now;  // Update the last run time
        runBilgeCycle();    // Execute the bilge cycle
    }

    // Optional: Handle NMEA2000 message triggers here
}

void runBilgeCycle() {
    for (int i = 0; i < NUM_ZONES; i++) {
        float peakCurrent = 0.0;           // Initialize peak current for this cycle
        unsigned long start = millis();    // Record the start time of the cycle
        bool retry = false;                // Flag to determine if a retry is needed

        digitalWrite(PUMP_PIN, HIGH);      // Turn on the pump
        delay(PUMP_PRIME_DELAY);           // Wait for the pump to prime

        digitalWrite(SOLENOID_PINS[i], HIGH);  // Open the solenoid valve for the current zone

        // Monitor the current draw during the cycle
        while (millis() - start < MAX_ZONE_RUNTIME) {
            float current = ina260.readCurrent();  // Read the current draw

            if (current > peakCurrent) {
                peakCurrent = current;  // Update peak current if a higher value is observed
            }

            if (current > CURRENT_THRESHOLD) {
                // Wait until the current drops below the threshold or timeout occurs
                while (ina260.readCurrent() > CURRENT_THRESHOLD) {
                    delay(500);  // Wait before checking again

                    if (millis() - start >= MAX_ZONE_RUNTIME) {
                        break;  // Exit if maximum runtime is exceeded
                    }
                }
                break;  // Exit the monitoring loop
            }

            delay(200);  // Wait before the next current reading
        }

        digitalWrite(SOLENOID_PINS[i], LOW);  // Close the solenoid valve
        digitalWrite(PUMP_PIN, LOW);          // Turn off the pump
        delay(1000);                          // Wait before proceeding

        // Create a log entry for this cycle
        ZoneLog log = {
            now(),                             // Timestamp of the log
            peakCurrent,                       // Peak current observed
            (uint16_t)(millis() - start),      // Duration of the cycle
            0                                  // Default result code (OK)
        };

        // Determine the result code based on peak current and duration
        if (peakCurrent < CURRENT_THRESHOLD / 2.0) {
            log.result = 1;  // DRY: Not enough water detected
            retry = true;    // Set retry flag
        } else if (millis() - start >= MAX_ZONE_RUNTIME) {
            log.result = 2;  // TIMEOUT: Cycle took too long
            retry = true;    // Set retry flag
        }

        if (retry) {
            delay(2000);  // Wait before retrying (optional retry logic can be added here)
        }

        EEPROM.put(i * sizeof(ZoneLog), log);  // Store the log in EEPROM
        sendZoneStatusN2K(i, log);             // Send the zone status over NMEA2000
    }
}
