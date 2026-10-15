// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents a pattern which is an expression interpreted as a value. Either a literal,
	 * block expression or an identifier.
	 */
	class ValuePattern final: public AnalysisPattern {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ValuePattern, AnalysisPattern);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(expression, ValuePatternExprHolder);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit ValuePattern(LangElementConstructionArgument state): AnalysisPattern(state) {
			this->element_kind = ElementKind::ValuePattern;
		}

		~ValuePattern() final = default;

		static MBox<ValuePattern> parse(LangParserState& state);
		void                      dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Value Pattern";
		}

		[[nodiscard]]
		AccessLocked<ValuePatternExprHolder> getExpression() const;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
