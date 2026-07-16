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
