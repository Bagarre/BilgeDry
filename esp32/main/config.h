// File: config.h
// Default configuration values for BilgeDry


#ifndef BILGEDRY_CONFIG_H
#define BILGEDRY_CONFIG_H

// Run scheduling (in hours)
#define DEFAULT_RUN_FREQUENCY_H 1

// Delays and thresholds (in seconds and amps)
#define DEFAULT_RUN_DELAY_S 3
#define DEFAULT_RUN_CURRENT_A 2.0f
#define DEFAULT_IDLE_CURRENT_A 0.5f

#define DEFAULT_RUN_MAX_S 10

// Zone names and enabled flags
#define DEFAULT_ZONE1_NAME "Zone 1"
#define DEFAULT_ZONE1_ENABLED true
#define DEFAULT_ZONE2_NAME "Zone 2"
#define DEFAULT_ZONE2_ENABLED false
#define DEFAULT_ZONE3_NAME "Zone 3"
#define DEFAULT_ZONE3_ENABLED true
#define DEFAULT_ZONE4_NAME "Zone 4"
#define DEFAULT_ZONE4_ENABLED true

#endif // BILGEDRY_CONFIG_H
