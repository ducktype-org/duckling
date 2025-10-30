#pragma once

#include <global_state/artifacts_location.hpp>

namespace compiler::driver {
    /**
     * Flush global artifacts and persist additional driver-managed data (e.g. query graph)
     */
    void saveArtifacts();
}
