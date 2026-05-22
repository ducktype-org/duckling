#pragma once

#include "binary_operator.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Logical `and` operator.
	 */
	class LogicAnd final: public BinaryOperator {
		using Lower = LogicNot;
		using Self  = LogicAnd;

	public:
		explicit LogicAnd(const LangParserState& state): BinaryOperator(state, 730) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~LogicAnd() override = default;
	};
}
