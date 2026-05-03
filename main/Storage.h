#pragma once

#include "AppTypes.h"

namespace Storage {
bool begin();
void loadDefaults(AppState& state);
bool loadAll(AppState& state);
bool saveRuntimeConfig(const AppState& state);
bool saveNetworkConfig(const AppState& state);
bool saveAll(const AppState& state);
}
