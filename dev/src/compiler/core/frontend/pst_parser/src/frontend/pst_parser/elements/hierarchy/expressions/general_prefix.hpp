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
		using Lower = ChainExpr;
		using Self  = GeneralPrefix;

	public:
		explicit GeneralPrefix(const LangParserState& state): PrefixOperator(state, 400) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~GeneralPrefix() override = default;
	};
}
