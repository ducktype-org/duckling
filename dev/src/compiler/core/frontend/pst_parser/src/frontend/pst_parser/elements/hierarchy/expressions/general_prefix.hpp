#pragma once

#include "expr_common.hpp"
#include "prefix_operator.hpp"

namespace pst::expr {
	/**
	 * @brief General prefix operator
	 *
	 * Excludes `not`
	 */
	class GeneralPrefix final: public PrefixOperator {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(GeneralPrefix, PrefixOperator);
		using Lower = ChainExpr;
		using Self  = GeneralPrefix;

	public:
		explicit GeneralPrefix(LangElementConstructionArgument state): PrefixOperator(state, 400) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~GeneralPrefix() override = default;
	};
}
