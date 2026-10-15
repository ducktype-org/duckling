// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "binary_operator.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Logical `or` operator.
	 */
	class LogicOr final: public BinaryOperator {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(LogicOr, BinaryOperator);

	protected:
		using Lower = LogicAnd;
		using Self  = LogicOr;

	public:
		explicit LogicOr(LangElementConstructionArgument state): BinaryOperator(state, 760) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~LogicOr() override = default;
	};
}
