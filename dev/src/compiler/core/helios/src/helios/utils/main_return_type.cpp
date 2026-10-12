// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "main_return_type.hpp"

#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>

namespace compiler::helios {
	tsh::SymbolType<> requiredMainReturnType(query::Context& ctx) {
		using enum tsh::IntegralAbstractType::Signedness;

		return tsh::SymbolType<>::withDefaults(tsh::getIntegralType(ctx, 64, Signed));
	}
}
