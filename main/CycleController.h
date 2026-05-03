#pragma once

#include "AppTypes.h"

namespace CycleController {
void begin(AppContext& ctx);
void scheduleFromNow(AppState& state);
void requestRun(AppContext& ctx);
void requestAbort(AppContext& ctx);
void tick(AppContext& ctx);
}
