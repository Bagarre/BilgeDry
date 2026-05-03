#pragma once

#include "AppTypes.h"

namespace LogManager {
bool begin();
void append(const String& level, const String& message);
void info(const String& message);
void warn(const String& message);
void error(const String& message);
String readAsJsonArray(size_t limit = kLogCapacity);
void clear();
}
