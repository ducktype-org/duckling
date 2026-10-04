#include "main_return_type.hpp"

#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>

namespace compiler::helios {
	tsh::SymbolType<> requiredMainReturnType(query::Context& ctx) {
		using enum tsh::IntegralAbstractType::Signedness;

		return tsh::SymbolType<>::withDefaults(tsh::getIntegralType(ctx, 64, Signed));
	}
}
