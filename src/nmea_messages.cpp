/*
 * BilgeDry - NMEA2000 Messaging Module
 * -------------------------------------
 * nmea_messages.cpp
 *
 * Description:
 *   This module handles the formatting and transmission of NMEA 2000 messages
 *   for the BilgeDry system. It includes functions to send status updates and
 *   zone-specific logs over the NMEA2000 network, utilizing PGN 130822.
 *
 * Functionality:
 *   - sendZoneStatusN2K: Transmits zone-specific log data including timestamp,
 *     peak current, duration, and result code.
 *   - sendN2KStatus: Sends a simplified system status message with a single-character code.
 *
 * Dependencies:
 *   - NMEA2000 library for message transmission
 *   - config.h for system configuration
 *   - nmea_messages.h for function declarations
 *
 * Author: Bagarre
 * Repository: https://github.com/Bagarre/BilgeDry
 * Date: 2025-05-05
 */

#include "nmea_messages.h"  // Header file for NMEA message functions
#include "config.h"         // Configuration settings for the BilgeDry system

// Function to send detailed status of a specific zone over NMEA2000
void sendZoneStatusN2K(int zoneId, const ZoneLog &log) {
    tN2kMsg N2kMsg;  // Create a new NMEA2000 message object

    // Populate the message with zone-specific data using PGN 130822
    SetN2kPGN130822(N2kMsg, zoneId, log.timestamp, log.peakCurrent, log.duration, log.result);

    // Transmit the message over the NMEA2000 network
    NMEA2000.SendMsg(N2kMsg);
}

// Function to send a simplified system status message over NMEA2000
void sendN2KStatus(const char *msg) {
    tN2kMsg N2kMsg;  // Create a new NMEA2000 message object

    // Populate the message with a single-character status code
    // Zone ID is set to 255 to indicate a system-wide message
    // Timestamp is the current system uptime in milliseconds
    // Peak current and duration are set to 0 as they are not applicable
    // The first character of the msg string is used as the result code
    SetN2kPGN130822(N2kMsg, 255, millis(), 0, 0, msg[0]);

    // Transmit the message over the NMEA2000 network
    NMEA2000.SendMsg(N2kMsg);
}
