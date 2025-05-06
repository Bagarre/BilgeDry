/*
 * BilgeDry - NMEA2000 Messaging Header
 * -------------------------------------
 * nmea_messages.h
 *
 * Description:
 *   This header file declares functions for formatting and transmitting
 *   NMEA 2000 messages within the BilgeDry system. It provides interfaces
 *   to send detailed zone status updates and simplified system status messages
 *   over the NMEA2000 network.
 *
 * Functionality:
 *   - sendZoneStatusN2K: Sends zone-specific log data including timestamp,
 *     peak current, duration, and result code.
 *   - sendN2KStatus: Sends a simplified system status message with a single-character code.
 *
 * Dependencies:
 *   - NMEA2000 library for message transmission
 *   - config.h for system configuration
 *
 * Author: Bagarre
 * Repository: https://github.com/Bagarre/BilgeDry
 * Date: 2025-05-05
 */

#pragma once  // Prevent multiple inclusions of this header file

#include "config.h"  // Include system configuration settings

// Sends a detailed status message for a specific zone over NMEA2000
// Parameters:
//   - zoneId: Identifier for the zone
//   - log:    Struct containing timestamp, peak current, duration, and result code
void sendZoneStatusN2K(int zoneId, const ZoneLog &log);

// Sends a simplified system status message over NMEA2000
// Parameters:
//   - msg: Pointer to a character array containing the status message
void sendN2KStatus(const char *msg);
