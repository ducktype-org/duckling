#pragma once

#include "expr_common.hpp"
#include "suffix_operator.hpp"

namespace pst::expr {
	/**
	 * @brief General suffix operator
	 */
	class GeneralSuffix final: public SuffixOperator {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(GeneralSuffix, SuffixOperator);
	protected:
		using Lower = GeneralPrefix;
		using Self  = GeneralSuffix;

		static MBox<ExprElement> parseRecursive(LangParserState& state, u64 iter);

	public:
		explicit GeneralSuffix(const LangParserState& state): SuffixOperator(state, 450) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~GeneralSuffix() override = default;
	};
}
