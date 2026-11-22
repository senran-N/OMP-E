#pragma once

#include "omp/client.h"
#include "omp/config_store.h"

namespace ompe {

// Hydrates realistic OMP state for deterministic --headless visual verification & offline demo mode.
void populate_showcase_fixture(OmpClient& client);
void populate_showcase_sessions(std::vector<SessionListItem>& out_sessions);

} // namespace ompe
