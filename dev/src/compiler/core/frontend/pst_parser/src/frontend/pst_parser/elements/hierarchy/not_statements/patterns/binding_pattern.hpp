// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include "analysis_pattern.hpp"

namespace pst {
	/**
	 * @brief Represents an binding pattern, which binds a value to a new variable.
	 */
	class BindingPattern final: public AnalysisPattern {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(BindingPattern, AnalysisPattern, is_let);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(name, IdentifierWrapper);
		bool is_let = false;

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit BindingPattern(LangElementConstructionData state): AnalysisPattern(state) {
			this->element_kind = ElementKind::BindingPattern;
		}

		~BindingPattern() final = default;

		static MBox<BindingPattern> parse(LangParserState& state);
		void                        dprint(std::ostream& out) const final;

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		/**
		 * @brief Whether the binding is declared with `let`.
		 */
		[[nodiscard]]
		bool isLet() const {
			return is_let;
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Binding Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
