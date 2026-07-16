#pragma once

#include "../../meta.hpp"
#include "template_expansion_assignment.hpp"

namespace pst {
	/**
	 * @brief Special top-level element for instantiated templates.
	 */
	class TemplateTopLevel final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(TemplateTopLevel, NotStmt);
		CLONE_SUBELEMENTS();

	protected:
		std::vector<AccessInternalAnonymous<TemplateExpansionAssignment>> assignments;
		NAMED_CHILD(stmt, Stmt);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

		friend class ElementSynthesizer;

	public:
		TemplateTopLevel(LangParserElementConstructionData data): NotStmt(data) {}

		void dprint(std::ostream& out) const final;
		~TemplateTopLevel() final = default;

		[[nodiscard]]
		usize numberOfAssignments() const {
			return assignments.size();
		}

		[[nodiscard]]
		AccessLocked<TemplateExpansionAssignment> getAssignmentByIndex(usize index) const {
			return assignments[index].give();
		}

		[[nodiscard]]
		AccessLocked<Stmt> getStmt() const {
			return stmt.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Template Top Level";
		}
	};
}
