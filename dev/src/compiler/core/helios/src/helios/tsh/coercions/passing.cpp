// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "passing.hpp"

#include <base/except/exceptions.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::tsh::coercions {
	PassingMethod passingMethod(query::Context& ctx, const ExpressionType<>& value) {
		const SymbolType<> symbol_type = value.getSymbolType();

		// A value that is being moved already is handed over as it is.
		if (value.getValueCategory().mustMove()) return PassingMethod::ByteCopy;

		switch (value.getValueCategory().getCategory()) {
		case PrimaryCategory::Temporary:
			// Purely an optimization, which keeps the generated code free of `implicit_move` on
			// everything. A trivially destructible temporary owns nothing to hand over, and moving
			// out of it would produce exactly the same code as copying its bytes.
			if (isCopiedByBytes(ctx, symbol_type)) return PassingMethod::ByteCopy;
			if (value.getValueCategory().isMovableFrom()) return PassingMethod::ImplicitMove;
			return PassingMethod::ExplicitCopyOrMove;
		case PrimaryCategory::Literal:
			return PassingMethod::ByteCopy;
		case PrimaryCategory::Local:
		case PrimaryCategory::Global:
		case PrimaryCategory::Dereferenced:
			if (symbol_type.isTriviallyCopyable(ctx)) return PassingMethod::ByteCopy;
			return symbol_type.isCopyable(ctx) ? PassingMethod::ExplicitCopyOrMove
			                                   : PassingMethod::NotCopyable;
		default:
			CORE_UNREACHABLE();
		}
	}

	bool isCopiedByBytes(query::Context& ctx, const SymbolType<>& type) {
		return type.isTriviallyCopyable(ctx) && type.isTriviallyDestructible(ctx);
	}
}
