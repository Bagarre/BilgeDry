#include "nmea_messages.h"
#include "config.h"

void sendZoneStatusN2K(int zoneId, const ZoneLog &log) {
  tN2kMsg N2kMsg;
  SetN2kPGN130822(N2kMsg, zoneId, log.timestamp, log.peakCurrent, log.duration, log.result);
  NMEA2000.SendMsg(N2kMsg);
}

void sendN2KStatus(const char *msg) {
  tN2kMsg N2kMsg;
  SetN2kPGN130822(N2kMsg, 255, millis(), 0, 0, msg[0]);  // Simplified status msg
  NMEA2000.SendMsg(N2kMsg);
}
