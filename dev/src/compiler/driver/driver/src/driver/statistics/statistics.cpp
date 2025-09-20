#include "statistics.hpp"

#include <driver_private/statistics_private/statistics.hpp>

namespace compiler::driver {
    timer::Duration getBackendCompilationTime() {
        return backend_compilation_time;
    }

}
