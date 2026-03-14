#pragma once

#include "expr_common.hpp"
#include "prefix_operator.hpp"

namespace pst::expr {
	/**
	 * @brief Logical `not` operator.
	 */
	class LogicNot final: public PrefixOperator {
		using Lower = ComparisonChain;
		using Self  = LogicNot;

	public:
		explicit LogicNot(const LangParserState& state):
			  PrefixOperator(state, lang_def::keywordToStr(Keyword::Not), 730) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~LogicNot() override = default;
	};
}
