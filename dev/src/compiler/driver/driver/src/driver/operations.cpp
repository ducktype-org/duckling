#include "operations.hpp"
#include <utility>

namespace driver {
    base::Bit256 KeyOf_CompileModule::queryUnstablePerfectHash() const {
        return { module_id.asInt(), std::to_underlying(backend_type) };
    }
}
