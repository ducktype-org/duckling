

#pragma once

#include "../lir_structure/lir_structure.hpp"

#include <mir/mir_structure/mir_structure.hpp>

namespace compiler::lir {
    LIRUnit lowerToLIRUnit(query::Context& ctx, const mir::MIRUnit& mir_unit);
}

