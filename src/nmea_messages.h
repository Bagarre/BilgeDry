#pragma once

#include <NMEA2000_CAN.h>

void sendZoneStatusN2K(int zoneId, const ZoneLog &log);
void sendN2KStatus(const char *msg);
