#include "LogManager.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>

auto constexpr kLogPath = "/logs.txt";

namespace {
String makeLine(const String& level, const String& message) {
  const time_t now = time(nullptr);
  return String(static_cast<unsigned long>(now)) + "|" + level + "|" + message;
}
}

namespace LogManager {

bool begin() {
  return true;
}

void append(const String& level, const String& message) {
  File f = SPIFFS.open(kLogPath, FILE_APPEND);
  if (!f) {
    return;
  }
  f.println(makeLine(level, message));
  f.close();
}

void info(const String& message) { append("INFO", message); }
void warn(const String& message) { append("WARN", message); }
void error(const String& message) { append("ERROR", message); }

String readAsJsonArray(size_t limit) {
  DynamicJsonDocument doc(12288);
  JsonArray arr = doc.to<JsonArray>();

  File f = SPIFFS.open(kLogPath, FILE_READ);
  if (!f) {
    String out;
    serializeJson(arr, out);
    return out;
  }

  String all = f.readString();
  f.close();

  int lineCount = 0;
  for (size_t i = 0; i < all.length(); ++i) {
    if (all[i] == '\n') {
      lineCount++;
    }
  }

  int skip = lineCount > static_cast<int>(limit) ? lineCount - static_cast<int>(limit) : 0;
  int seen = 0;
  int start = 0;

  while (start < all.length()) {
    int end = all.indexOf('\n', start);
    if (end < 0) {
      end = all.length();
    }

    String line = all.substring(start, end);
    line.trim();
    if (!line.isEmpty()) {
      if (seen >= skip) {
        const int first = line.indexOf('|');
        const int second = line.indexOf('|', first + 1);
        JsonObject entry = arr.createNestedObject();
        if (first > 0 && second > first) {
          entry["epoch"] = line.substring(0, first).toInt();
          entry["level"] = line.substring(first + 1, second);
          entry["message"] = line.substring(second + 1);
        } else {
          entry["epoch"] = 0;
          entry["level"] = "INFO";
          entry["message"] = line;
        }
      }
      seen++;
    }
    start = end + 1;
  }

  String out;
  serializeJson(arr, out);
  return out;
}

void clear() {
  SPIFFS.remove(kLogPath);
}

}
