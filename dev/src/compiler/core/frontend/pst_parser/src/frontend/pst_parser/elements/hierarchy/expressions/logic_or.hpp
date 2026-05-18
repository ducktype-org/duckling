#pragma once

#include "binary_operator.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Logical `or` operator.
	 */
	class LogicOr final: public BinaryOperator {
		using Lower = LogicAnd;
		using Self  = LogicOr;

	public:
		explicit LogicOr(const LangParserState& state): BinaryOperator(state, 760) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~LogicOr() override = default;
	};
}
