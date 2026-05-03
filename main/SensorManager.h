#pragma once

#include "AppTypes.h"

namespace SensorManager {
void begin(AppContext& ctx);
float readCurrentAmps(AppContext& ctx);
}
