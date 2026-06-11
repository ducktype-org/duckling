#pragma once

#include "../not_statements/wrapper_elements/operator_wrapper.hpp"
#include "expr_common.hpp"
#include "prefix_operator.hpp"

namespace pst::expr {
	/**
	 * @brief Logical `not` operator.
	 */
	class LogicNot final: public PrefixOperator {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(LogicNot, PrefixOperator);
	protected:
		using Lower = ComparisonChain;
		using Self  = LogicNot;

	public:
		explicit LogicNot(const LangParserState& state): PrefixOperator(state, 730) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~LogicNot() override = default;
	};
}
