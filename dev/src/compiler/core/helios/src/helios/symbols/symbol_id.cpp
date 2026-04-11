#include "symbol_id.hpp"

#include <helios_private/symbols/symbol_data.hpp>


namespace compiler::helios {

	u64 SymID::queryUnstablePerfectHash() const {
        return ref->id.asInt();
    }
}
