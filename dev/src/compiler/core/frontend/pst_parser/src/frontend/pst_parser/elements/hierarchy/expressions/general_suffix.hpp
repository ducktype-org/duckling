// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
		explicit GeneralSuffix(LangElementConstructionArgument state): SuffixOperator(state, 450) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~GeneralSuffix() override = default;
	};
}
