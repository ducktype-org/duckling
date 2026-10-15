// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../../meta.hpp"

namespace pst {
	/**
	 * @brief Special top-level element for instantiated templates.
	 */
	class TemplateExpansionAssignment final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(TemplateExpansionAssignment, NotStmt);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(name, IdentifierWrapper);

		// These are generic ExprHolders as this class doesn't take part in parsing.
		NAMED_CHILD_OPT(type, ExprHolder);  // Not sure if the type can be deductible
		NAMED_CHILD(value, ExprHolder);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

		friend class ElementSynthesizer;

	public:
		TemplateExpansionAssignment(LangParserElementConstructionData data): NotStmt(data) {}

		void dprint(std::ostream& out) const final;
		~TemplateExpansionAssignment() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Template Expansion Assignment";
		}
	};
}
