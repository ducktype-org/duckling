// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "value_category.hpp"

#include <helios/symbols/symbol_kind.hpp>

namespace compiler::tsh {
	PrimaryCategory primaryCategoryOfSymbol(query::Context& ctx, compiler::helios::SymID symbol) {
		compiler::helios::SymbolKind symbol_kind = kind(symbol);

		// Parameters are owned locals (the function owns them, so they can be moved from).
		if (symbol_kind == compiler::helios::SymbolKind::Parameter) return PrimaryCategory::Local;

		// A `Variable` may be either a local or a global variable.
		if (symbol_kind == compiler::helios::SymbolKind::Variable)
			return compiler::helios::isGlobalVar(ctx, symbol) ? PrimaryCategory::Global
			                                                  : PrimaryCategory::Local;

		// Everything else is a global.
		return PrimaryCategory::Global;
	}

	/**
	 * @brief Construct the value category with default attributes based on primary category.
	 * @param pc The primary category.
	 */
	ValueCategory::ValueCategory(const PrimaryCategory& pc) {
		// @TODO
		// Default values might need some tweaking in the future
		switch (pc) {
		case PrimaryCategory::Temporary:
			category        = PrimaryCategory::Temporary;
			is_pure         = true;
			allows_semantic = MOVE | COPY | USE | DESTROY;  // All but REINIT
			break;
		case PrimaryCategory::Local:
			category        = PrimaryCategory::Local;
			is_pure         = false;
			allows_semantic = MOVE | COPY | REINIT | USE | DESTROY | REFERENCE;  // All
			break;
		case PrimaryCategory::Global:
			category        = PrimaryCategory::Global;
			is_pure         = false;
			allows_semantic = COPY | REINIT | USE | REFERENCE;  // All but MOVE and DESTROY
			break;
		case PrimaryCategory::Dereferenced:
			category = PrimaryCategory::Dereferenced;
			// A dereferenced location may alias, so it is not pure. It is a non-owned lvalue. It
			// can be read and assigned to, but not moved out of.
			is_pure = false;
			allows_semantic
				= COPY | REINIT | USE
			    | REFERENCE;  // Same as Global (but it's not a global) - non-owned lvalue.
			break;
		case PrimaryCategory::Literal:
			category        = PrimaryCategory::Literal;
			is_pure         = true;
			allows_semantic = COPY | USE | DESTROY;  // All but MOVE and REINIT
			break;
		}
	}

	/**
	 * @brief Construct the value category with manually given values of all attributes.
	 */
	ValueCategory::ValueCategory(
		const PrimaryCategory category,
		const bool            is_pure,
		const ValueSemantics  allows_semantic,
		const ValueSemantics  force_semantic
	):
		  category(category),
		  is_pure(is_pure),
		  allows_semantic(allows_semantic),
		  force_semantic(force_semantic) {}

}
