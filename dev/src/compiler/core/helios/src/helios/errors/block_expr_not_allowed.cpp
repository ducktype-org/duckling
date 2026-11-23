// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "block_expr_not_allowed.hpp"

#include <helios/utils/hout_walkers.hpp>

#include <base/collections/optional.hpp>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <query_framework/query_errors.hpp>

namespace compiler::helios {

	BlockExprNotAllowedError::BlockExprNotAllowedError(
		dia::StablePosition source_position, std::string place
	):
		  MessageWithCodeFragmentAndCause(source_position) {
		addArgument<dia::TextArgument>("place", std::move(place));
	}

	void refuseBlockExpr(query::Context& ctx, const code::Expr& expr, std::string_view place) {
		if_opt_some(code::findBlockExpr(expr), block_expr) {
			ctx.log<BlockExprNotAllowedError>(block_expr->getPosition().value(), std::string(place));
			query::throwFailed();
		}
	}
}
