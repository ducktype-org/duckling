// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/hout/elements/expr.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <base/collections/optional.hpp>

namespace compiler::helios {
	/**
	 * If expression is an identifier expression, returns its symbol ID.
	 * otherwise returns an empty optional.
	 */
	base::Optional<SymID> getIdentifierExprSymID(CRef<code::Expr> expr);
}
