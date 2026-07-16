#pragma once

#include "binary_operator.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Logical `and` operator.
	 */
	class LogicAnd final: public BinaryOperator {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(LogicAnd, BinaryOperator);

	protected:
		using Lower = LogicNot;
		using Self  = LogicAnd;

	public:
		explicit LogicAnd(LangElementConstructionArgument state): BinaryOperator(state, 730) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~LogicAnd() override = default;
	};
}
