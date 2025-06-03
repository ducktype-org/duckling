#pragma once

#include "preamble.hpp"
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
		explicit GeneralPrefix(const dia::SourcePosition& pos, Operator op):
			  PrefixOperator(pos, op, 400) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~GeneralPrefix() override = default;
	};
}
