// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/hout/elements/expr.hpp>

#include <diagnostic/message.hpp>
#include <diagnostic/stable_position.hpp>
#include <query_framework/context/context.hpp>

#include <string>
#include <string_view>

namespace compiler::helios {

	/**
	 * @brief Error reported for a block expression in a place that is evaluated outside of the
	 * function it is written in, like a default argument or a global initializer.
	 */
	class BlockExprNotAllowedError final: public dia::MessageWithCodeFragmentAndCause {
		[[nodiscard]] dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "block_expr_not_allowed" };
		}

	public:
		BlockExprNotAllowedError(dia::StablePosition source_position, std::string place);
	};

	void refuseBlockExpr(query::Context& ctx, const code::Expr& expr, std::string_view place);

}
